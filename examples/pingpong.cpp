//
// pingpong.cpp - synchronous ping-pong benchmark
// Client: send one msg, wait for echo, send next (true ping-pong)
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

#include <atomic>
#include <vector>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cinttypes>

// ─── config ───
static int    SERVER_PORT  = 8000;
static const char* SERVER_IP = "127.0.0.1";
static int    CONNS        = 100;
static int    THREADS      = 4;
static int    DURATION_SEC = 10;
static int    MSG_SIZE     = 64;
static const int HEADER_SIZE  = 4;
static const int EPOLL_SIZE   = 4096;

// ─── helpers ───
static void encodeLen(char* buf, int32_t len) {
    int32_t be = htobe32(len);
    memcpy(buf, &be, sizeof(be));
}
static int32_t decodeLen(const char* buf) {
    int32_t be; memcpy(&be, buf, sizeof(be)); return be32toh(be);
}
static int setNonblock(int fd) {
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

// ─── stats ───
struct Stats {
    std::atomic<int64_t> msgs{0};
    std::atomic<int64_t> bytes{0};
    std::atomic<bool>    running{true};
};
static Stats g_stats;

// ─── per-connection state ───
enum RecvState { READING_HEADER, READING_PAYLOAD };

struct Conn {
    int fd;
    char send_buf[HEADER_SIZE + 65536];
    int  send_msg_size;  // HEADER_SIZE + MSG_SIZE
    RecvState rs;
    int  recv_off, recv_need;
    char rhdr[HEADER_SIZE];
    int64_t msgs, bytes;
};

// ─── worker ───
static void worker(int tid, int conns_per_thread) {
    int epfd = epoll_create1(EPOLL_CLOEXEC);

    std::vector<Conn> conns(conns_per_thread);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    for (int i = 0; i < conns_per_thread; ++i) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        setNonblock(fd);
        int one = 1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        connect(fd, (struct sockaddr*)&addr, sizeof(addr));

        Conn& c = conns[i];
        memset(&c, 0, sizeof(c));
        c.fd = fd;
        encodeLen(c.send_buf, MSG_SIZE);
        memset(c.send_buf + HEADER_SIZE, 'A', MSG_SIZE);
        c.send_msg_size = HEADER_SIZE + MSG_SIZE;
        c.rs = READING_HEADER;
        c.recv_need = HEADER_SIZE;

        epoll_event ev;
        ev.events = EPOLLOUT | EPOLLET;
        ev.data.ptr = &c;
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    }

    epoll_event events[EPOLL_SIZE];

    while (g_stats.running.load(std::memory_order_relaxed)) {
        int n = epoll_wait(epfd, events, EPOLL_SIZE, 100);
        if (n < 0 && errno != EINTR) break;

        for (int i = 0; i < n; ++i) {
            Conn* c = (Conn*)events[i].data.ptr;
            uint32_t rev = events[i].events;

            if (rev & (EPOLLERR | EPOLLHUP)) {
                epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                close(c->fd); c->fd = -1;
                continue;
            }

            if (rev & EPOLLOUT) {
                int err = 0; socklen_t elen = sizeof(err);
                getsockopt(c->fd, SOL_SOCKET, SO_ERROR, &err, &elen);
                if (err != 0) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                    close(c->fd); c->fd = -1;
                    continue;
                }
                // connected → send first ping
                ssize_t nw = write(c->fd, c->send_buf, c->send_msg_size);
                if (nw == c->send_msg_size) {
                    epoll_event ev;
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.ptr = c;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
                }
                // (partial writes handled via offset if needed — skipped for tiny msgs)
                continue;
            }

            if (rev & EPOLLIN) {
                while (true) {
                    char* dst   = (c->rs == READING_HEADER) ? c->rhdr
                                   : c->send_buf + HEADER_SIZE; // reuse buffer
                    int cap     = c->recv_need;
                    ssize_t nr  = read(c->fd, dst + c->recv_off, cap - c->recv_off);
                    if (nr > 0) {
                        c->recv_off += nr;
                        if (c->recv_off == cap) {
                            if (c->rs == READING_HEADER) {
                                int32_t plen = decodeLen(c->rhdr);
                                c->rs = READING_PAYLOAD;
                                c->recv_need = plen;
                                c->recv_off = 0;
                                // continue to read payload
                            } else {
                                // full response received (pong) → count + send next (ping)
                                c->msgs++;
                                c->bytes += HEADER_SIZE + c->recv_need; // inbound total
                                c->rs = READING_HEADER;
                                c->recv_need = HEADER_SIZE;
                                c->recv_off = 0;

                                // send next ping
                                ssize_t nw = write(c->fd, c->send_buf, c->send_msg_size);
                                if (nw != c->send_msg_size) {
                                    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                                    close(c->fd); c->fd = -1;
                                    goto next_event;
                                }
                            }
                        }
                    } else if (nr == 0) {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                        close(c->fd); c->fd = -1;
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) goto next_event;
                        epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                        close(c->fd); c->fd = -1;
                        break;
                    }
                }
            next_event:;
            }
        }
    }

    // aggregate
    int64_t msgs = 0, bytes = 0;
    for (auto& c : conns) { if (c.fd >= 0) close(c.fd); msgs += c.msgs; bytes += c.bytes; }
    close(epfd);
    g_stats.msgs.fetch_add(msgs, std::memory_order_relaxed);
    g_stats.bytes.fetch_add(bytes, std::memory_order_relaxed);
}

// ─── main ───
int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-c") == 0 && i+1 < argc) CONNS = atoi(argv[++i]);
        else if (strcmp(argv[i], "-t") == 0 && i+1 < argc) THREADS = atoi(argv[++i]);
        else if (strcmp(argv[i], "-d") == 0 && i+1 < argc) DURATION_SEC = atoi(argv[++i]);
        else if (strcmp(argv[i], "-s") == 0 && i+1 < argc) MSG_SIZE = atoi(argv[++i]);
        else if (strcmp(argv[i], "-p") == 0 && i+1 < argc) SERVER_PORT = atoi(argv[++i]);
    }

    int cpt = CONNS / THREADS;
    printf("=== Ping-Pong Benchmark ===\n");
    printf("Server:     %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("Conns:      %d (%d threads x %d conns)\n", CONNS, THREADS, cpt);
    printf("Msg size:   %d bytes\n", MSG_SIZE);
    printf("Duration:   %d s\n\n", DURATION_SEC);

    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t)
        threads.emplace_back(worker, t, cpt);

    printf("Warming up 2s...\n");
    sleep(2);
    int64_t base_msgs  = g_stats.msgs.load();
    int64_t base_bytes = g_stats.bytes.load();
    int64_t t0 = time(nullptr);

    sleep(DURATION_SEC);

    g_stats.running.store(false);
    for (auto& t : threads) t.join();

    int64_t total_msgs  = g_stats.msgs.load() - base_msgs;
    int64_t total_bytes = g_stats.bytes.load() - base_bytes;
    double elapsed = time(nullptr) - t0;
    double avg_msg_size = total_msgs > 0 ? (double)total_bytes / total_msgs : 0;
    double mib_per_s = (total_bytes / 1048576.0) / elapsed;

    printf("\n=== Results ===\n");
    printf("total bytes read:      %" PRId64 "\n", total_bytes);
    printf("total messages read:   %" PRId64 "\n", total_msgs);
    printf("average message size:  %.1f bytes\n", avg_msg_size);
    printf("MiB/s throughput:      %.2f MiB/s\n", mib_per_s);

    return 0;
}
