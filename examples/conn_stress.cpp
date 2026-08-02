//
// conn_stress.cpp - 10k concurrent connections stress test
//

#include "jpmuduo/net/TcpServer.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/base/Logger.h"

#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <chrono>

using namespace jpmuduo;

static const int kTotalConns = 10000;
static const uint16_t kPort = 19999;

// ─── Server ───────────────────────────────────────────────────────
static std::atomic<int> s_serverAccepted{0};
static std::atomic<int> s_serverClosed{0};

class StressServer {
public:
    StressServer(EventLoop* loop, const InetAddress& addr)
        : server_(loop, addr, "StressServer") {
        server_.setConnectionCallback([this](const TcpConnectionPtr& conn) {
            if (conn->connected()) {
                s_serverAccepted.fetch_add(1);
            } else {
                s_serverClosed.fetch_add(1);
            }
        });
        server_.setMessageCallback([](const TcpConnectionPtr&, Buffer*, TimeStamp) {});
        server_.setThreadNum(8);
    }
    void start() { server_.start(); }

private:
    TcpServer server_;
};

// ─── Client (raw non-blocking sockets) ────────────────────────────
static int createNonblockSocket() {
    return ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
}

int main() {
    // Ensure we have enough fds
    struct rlimit rl = {1048576, 1048576};
    ::setrlimit(RLIMIT_NOFILE, &rl);

    // ── Start server ──
    std::thread serverThread([]() {
        EventLoop loop;
        InetAddress addr(kPort);
        StressServer server(&loop, addr);
        server.start();
        loop.loop();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // ── Create 10k non-blocking sockets, connect all ──
    struct sockaddr_in saddr;
    ::bzero(&saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(kPort);
    ::inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr);

    std::vector<int> fds(kTotalConns, -1);
    std::atomic<int> connSuccess{0};
    std::atomic<int> connInProgress{0};
    std::atomic<int> connError{0};

    auto t0 = std::chrono::steady_clock::now();

    // Phase 1: fire all connect() calls
    for (int i = 0; i < kTotalConns; ++i) {
        int fd = createNonblockSocket();
        if (fd < 0) {
            connError.fetch_add(1);
            continue;
        }
        fds[i] = fd;

        int ret = ::connect(fd, (struct sockaddr*)&saddr, sizeof(saddr));
        if (ret == 0) {
            connSuccess.fetch_add(1);
        } else if (errno == EINPROGRESS) {
            connInProgress.fetch_add(1);
        } else {
            connError.fetch_add(1);
            ::close(fd);
            fds[i] = -1;
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    auto launchMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::cout << "Phase 1 (connect): " << launchMs << " ms" << std::endl;
    std::cout << "  immediate success: " << connSuccess.load() << std::endl;
    std::cout << "  in progress:       " << connInProgress.load() << std::endl;
    std::cout << "  errors:            " << connError.load() << std::endl;

    // Phase 2: poll for EINPROGRESS completions
    int pollDone = 0;
    int pollTimeout = 0;
    int pollError = 0;

    for (int i = 0; i < kTotalConns; ++i) {
        if (fds[i] < 0) continue;

        struct pollfd pfd;
        pfd.fd = fds[i];
        pfd.events = POLLOUT;

        int ret = ::poll(&pfd, 1, 2000); // 2 sec per fd
        if (ret > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            if (::getsockopt(fds[i], SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0) {
                pollDone++;
                connSuccess.fetch_add(1);
            } else {
                pollError++;
                connError.fetch_add(1);
                ::close(fds[i]);
                fds[i] = -1;
            }
        } else if (ret == 0) {
            pollTimeout++;
            connError.fetch_add(1);
            ::close(fds[i]);
            fds[i] = -1;
        } else {
            pollError++;
            connError.fetch_add(1);
            ::close(fds[i]);
            fds[i] = -1;
        }
    }

    auto t2 = std::chrono::steady_clock::now();
    auto pollMs = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::cout << "Phase 2 (poll EINPROGRESS): " << pollMs << " ms" << std::endl;
    std::cout << "  completed:  " << pollDone << std::endl;
    std::cout << "  timeout:    " << pollTimeout << std::endl;
    std::cout << "  error:      " << pollError << std::endl;

    // Wait for server to accept
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Phase 3: close all
    for (int i = 0; i < kTotalConns; ++i) {
        if (fds[i] >= 0) {
            ::close(fds[i]);
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // ── Results ──
    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Attempted:         " << kTotalConns << std::endl;
    std::cout << "Client success:    " << connSuccess.load() << std::endl;
    std::cout << "Client error:      " << connError.load() << std::endl;
    std::cout << "Server accepted:   " << s_serverAccepted.load() << std::endl;
    std::cout << "Server closed:     " << s_serverClosed.load() << std::endl;

    int lost = connSuccess.load() - s_serverAccepted.load();
    std::cout << "Lost (client ok - server accepted): " << lost << std::endl;
    std::cout << "Loss rate: " << (lost * 100.0 / kTotalConns) << "%" << std::endl;

    // Shutdown server
    serverThread.detach();
    return (lost == 0) ? 0 : 1;
}
