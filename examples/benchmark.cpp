// QPS benchmark client for muduoSelf
// Length-prefixed framing (4-byte header + payload)
// Multi-threaded, epoll-based, measures QPS and latency

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <endian.h>

#include <atomic>
#include <vector>
#include <thread>
#include <algorithm>
#include <chrono>
#include <string>

// ---------- config ----------
static int    SERVER_PORT   = 8000;
static const char* SERVER_IP = "127.0.0.1";
static int    CONNS         = 2000;
static int    THREADS       = 4;
static int    DURATION_SEC  = 30;
static int    MSG_SIZE      = 64;
static const int EPOLL_TIMEOUT = 100;
static const int MAX_EVENTS    = 4096;
static const int HEADER_SIZE   = 4;

// ---------- global stats ----------
std::atomic<int64_t> g_total_requests{0};
std::atomic<int64_t> g_total_responses{0};
std::atomic<int64_t> g_total_bytes{0};
std::atomic<bool>    g_running{true};

// latency recording
struct LatencyRecorder {
    static const int64_t MAX_US = 10000000;
    static const int BUCKETS = 20000;
    std::atomic<int64_t> buckets[BUCKETS]{};
    std::atomic<int64_t> count{0};
    std::atomic<int64_t> total_us{0};

    void record(int64_t us) {
        int idx = std::min<int64_t>(us / 500, BUCKETS - 1);
        buckets[idx].fetch_add(1, std::memory_order_relaxed);
        count.fetch_add(1, std::memory_order_relaxed);
        total_us.fetch_add(us, std::memory_order_relaxed);
    }

    void report() const {
        int64_t n = count.load();
        if (n == 0) return;
        int64_t local[BUCKETS];
        for (int i = 0; i < BUCKETS; ++i)
            local[i] = buckets[i].load();

        int64_t p50_idx = n * 50 / 100;
        int64_t p90_idx = n * 90 / 100;
        int64_t p99_idx = n * 99 / 100;
        int64_t p999_idx = n * 999 / 1000;
        int64_t cum = 0;
        int64_t p50_us = 0, p90_us = 0, p99_us = 0, p999_us = 0;
        for (int i = 0; i < BUCKETS; ++i) {
            cum += local[i];
            if (p50_us == 0 && cum >= p50_idx) p50_us = i * 500 + 250;
            if (p90_us == 0 && cum >= p90_idx) p90_us = i * 500 + 250;
            if (p99_us == 0 && cum >= p99_idx) p99_us = i * 500 + 250;
            if (p999_us == 0 && cum >= p999_idx) p999_us = i * 500 + 250;
        }
        double avg_us = (double)total_us.load() / n;
        printf("  Latency (us): avg=%.1f  p50=%ld  p90=%ld  p99=%ld  p999=%ld\n",
               avg_us, p50_us, p90_us, p99_us, p999_us);
    }
};
LatencyRecorder g_latency;

// ---------- helpers ----------
int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int64_t now_us() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

static void encodeLen(char* buf, int32_t len) {
    int32_t be = htobe32(len);
    memcpy(buf, &be, sizeof(be));
}

static int32_t decodeLen(const char* buf) {
    int32_t be;
    memcpy(&be, buf, sizeof(be));
    return be32toh(be);
}

// ---------- connection state ----------
enum RecvState { READING_HEADER, READING_PAYLOAD };

struct ConnState {
    int fd;
    int64_t send_ts_us;
    // send
    char send_buf[HEADER_SIZE + 65536];
    int  send_offset;
    int  send_len;       // HEADER_SIZE + MSG_SIZE
    // recv
    RecvState recv_state;
    int  recv_offset;    // bytes received in current phase
    int  recv_need;      // bytes needed in current phase
    char recv_header[HEADER_SIZE];
    char recv_payload[65536];
    // counters
    int64_t requests;
    int64_t responses;
};

// ---------- worker thread ----------
void worker_thread(int thread_id, int conns_per_thread) {
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    std::vector<ConnState> conns(conns_per_thread);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    for (int i = 0; i < conns_per_thread; ++i) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        set_nonblock(fd);

        int nodelay = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

        connect(fd, (struct sockaddr*)&addr, sizeof(addr));

        ConnState& c = conns[i];
        memset(&c, 0, sizeof(c));
        c.fd = fd;

        // Prepare send buffer: [4B length header][payload]
        encodeLen(c.send_buf, MSG_SIZE);
        memset(c.send_buf + HEADER_SIZE, 'A', MSG_SIZE);
        c.send_len = HEADER_SIZE + MSG_SIZE;
        c.recv_state = READING_HEADER;
        c.recv_need  = HEADER_SIZE;

        epoll_event ev;
        ev.events = EPOLLOUT | EPOLLET;
        ev.data.ptr = &c;
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    }

    epoll_event events[MAX_EVENTS];

    while (g_running.load(std::memory_order_relaxed)) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, EPOLL_TIMEOUT);
        if (n < 0 && errno != EINTR) break;

        for (int i = 0; i < n; ++i) {
            ConnState* c = (ConnState*)events[i].data.ptr;
            uint32_t revents = events[i].events;

            if (revents & (EPOLLERR | EPOLLHUP)) {
                epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                close(c->fd);
                c->fd = -1;
                continue;
            }

            if (revents & EPOLLOUT) {
                int err = 0;
                socklen_t elen = sizeof(err);
                getsockopt(c->fd, SOL_SOCKET, SO_ERROR, &err, &elen);
                if (err != 0) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                    close(c->fd);
                    c->fd = -1;
                    continue;
                }
                // connection established, send first request
                ssize_t nw = write(c->fd, c->send_buf + c->send_offset,
                                   c->send_len - c->send_offset);
                if (nw > 0) {
                    c->send_offset += nw;
                    if (c->send_offset == c->send_len) {
                        c->send_offset = 0;
                        c->send_ts_us = now_us();
                        c->requests++;
                        g_total_requests.fetch_add(1, std::memory_order_relaxed);
                        // switch to read
                        epoll_event ev;
                        ev.events = EPOLLIN | EPOLLET;
                        ev.data.ptr = c;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
                    }
                } else if (nw < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                    close(c->fd);
                    c->fd = -1;
                }
                continue;
            }

            if (revents & EPOLLIN) {
                int64_t recv_now = now_us();
                while (true) {
                    char* dst;
                    int   capacity;
                    if (c->recv_state == READING_HEADER) {
                        dst = c->recv_header;
                        capacity = HEADER_SIZE;
                    } else {
                        dst = c->recv_payload;
                        capacity = c->recv_need;
                    }

                    ssize_t nr = read(c->fd, dst + c->recv_offset,
                                      capacity - c->recv_offset);
                    if (nr > 0) {
                        c->recv_offset += nr;
                        g_total_bytes.fetch_add(nr, std::memory_order_relaxed);

                        if (c->recv_offset == capacity) {
                            if (c->recv_state == READING_HEADER) {
                                // got full header → switch to payload
                                int32_t payload_len = decodeLen(c->recv_header);
                                if (payload_len < 0 || payload_len > 65536) {
                                    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                                    close(c->fd);
                                    c->fd = -1;
                                    goto next_event;
                                }
                                c->recv_state = READING_PAYLOAD;
                                c->recv_need  = payload_len;
                                c->recv_offset = 0;
                                // continue loop to try reading payload
                            } else {
                                // got full payload → response complete
                                int64_t rtt = recv_now - c->send_ts_us;
                                g_latency.record(rtt);
                                c->recv_state = READING_HEADER;
                                c->recv_need  = HEADER_SIZE;
                                c->recv_offset = 0;
                                c->responses++;
                                g_total_responses.fetch_add(1, std::memory_order_relaxed);

                                // try direct write to skip epoll_ctl + epoll_wait cycle
                                c->send_offset = 0;
                                c->send_ts_us = recv_now;
                                {
                                    ssize_t nw = write(c->fd, c->send_buf, c->send_len);
                                    if (nw == c->send_len) {
                                        c->send_offset = 0;
                                        c->requests++;
                                        g_total_requests.fetch_add(1, std::memory_order_relaxed);
                                        // stay in EPOLLIN — no epoll_ctl needed
                                        continue;  // keep draining EPOLLIN
                                    } else if (nw > 0) {
                                        c->send_offset = nw;
                                    } else if (nw < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
                                        epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                                        close(c->fd); c->fd = -1;
                                        goto next_event;
                                    }
                                }
                                // socket full or partial write → fall back to EPOLLOUT
                                epoll_event ev;
                                ev.events = EPOLLOUT | EPOLLET;
                                ev.data.ptr = c;
                                epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
                                goto next_event;
                            }
                        }
                    } else if (nr == 0) {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                        close(c->fd);
                        c->fd = -1;
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            goto next_event;
                        epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                        close(c->fd);
                        c->fd = -1;
                        break;
                    }
                }
            next_event:;
            }
        }
    }

    for (auto& c : conns) {
        if (c.fd >= 0) close(c.fd);
    }
    close(epfd);
}

// ---------- main ----------
int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) CONNS = atoi(argv[++i]);
        else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) THREADS = atoi(argv[++i]);
        else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) DURATION_SEC = atoi(argv[++i]);
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) MSG_SIZE = atoi(argv[++i]);
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) SERVER_PORT = atoi(argv[++i]);
        else if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: benchmark [-c conns] [-t threads] [-d duration_sec] [-s msg_size] [-p port]\n");
            return 0;
        }
    }

    struct rlimit rlim;
    rlim.rlim_cur = 1000000;
    rlim.rlim_max = 1000000;
    setrlimit(RLIMIT_NOFILE, &rlim);

    int conns_per_thread = CONNS / THREADS;
    printf("=== muduoSelf QPS Benchmark (length-prefixed framing) ===\n");
    printf("Server:   %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("Conns:    %d (%d threads x %d conns)\n", CONNS, THREADS, conns_per_thread);
    printf("Msg size: %d bytes\n", MSG_SIZE);
    printf("Duration: %d seconds\n\n", DURATION_SEC);

    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t)
        threads.emplace_back(worker_thread, t, conns_per_thread);

    printf("Warming up for 2s...\n");
    sleep(2);

    int64_t start_req = g_total_requests.load();
    int64_t start_resp = g_total_responses.load();
    int64_t start_bytes = g_total_bytes.load();
    int64_t bench_start = now_us();

    printf("Benchmark running for %ds...\n", DURATION_SEC);

    for (int elapsed = 5; elapsed <= DURATION_SEC; elapsed += 5) {
        int64_t target = bench_start + elapsed * 1000000LL;
        int64_t sleep_us = target - now_us();
        if (sleep_us > 0) usleep(sleep_us);

        int64_t cur_time = now_us();
        int64_t cur_resp = g_total_responses.load() - start_resp;
        double secs = (cur_time - bench_start) / 1000000.0;
        printf("  [%2d/%2ds] resp=%ld QPS=%.0f\n",
               elapsed, DURATION_SEC, cur_resp, cur_resp / secs);
    }

    int64_t end_us = now_us();
    g_running.store(false);

    for (auto& t : threads) t.join();

    int64_t total_req = g_total_requests.load() - start_req;
    int64_t total_resp = g_total_responses.load() - start_resp;
    int64_t total_bytes = g_total_bytes.load() - start_bytes;
    double duration_sec = (end_us - bench_start) / 1000000.0;

    printf("\n=== Results ===\n");
    printf("Duration:       %.2f s\n", duration_sec);
    printf("Total requests:  %ld\n", total_req);
    printf("Total responses: %ld\n", total_resp);
    printf("Throughput:      %.0f req/s\n", total_resp / duration_sec);
    printf("Total data:      %.2f MB\n", total_bytes / 1048576.0);
    printf("Bandwidth:       %.2f MB/s\n", (total_bytes / 1048576.0) / duration_sec);
    g_latency.report();

    return 0;
}
