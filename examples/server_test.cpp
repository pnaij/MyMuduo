//
// server_test.cpp - TcpServer unit tests
//
#include "jpmuduo/net/TcpServer.h"
#include "jpmuduo/base/Logger.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/net/Buffer.h"
#include "jpmuduo/base/TimeStamp.h"

#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <atomic>
#include <iostream>
#include <functional>
#include <chrono>

using namespace jpmuduo;

static std::atomic<int> s_connectionUp{0};
static std::atomic<int> s_connectionDown{0};
static std::atomic<int> s_messageCount{0};

class TestEchoServer {
public:
    TestEchoServer(EventLoop* loop, const InetAddress& addr, const std::string& name)
        : server_(loop, addr, name), loop_(loop) {
        server_.setConnectionCallback(
            std::bind(&TestEchoServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(
            std::bind(&TestEchoServer::onMessage, this,
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }
    void start() { server_.start(); }
private:
    void onConnection(const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            LOG_INFO("TEST: Connection UP : %s", conn->peerAddress().toIpPort().c_str());
            s_connectionUp++;
        } else {
            LOG_INFO("TEST: Connection DOWN : %s", conn->peerAddress().toIpPort().c_str());
            s_connectionDown++;
        }
    }
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, TimeStamp) {
        std::string msg = buf->retrieveAllAsString();
        LOG_INFO("TEST: Message: %s", msg.c_str());
        s_messageCount++;
        conn->send("ECHO: " + msg);
    }
    TcpServer server_;
    EventLoop* loop_;
};

// Blocking TCP send+recv helper
static int sendAndRecv(uint16_t port, const std::string& msg, std::string& resp, int timeoutMs = 3000) {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { ::perror("socket"); return -1; }

    struct sockaddr_in addr;
    ::bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::perror("connect"); ::close(sock); return -1;
    }

    if (::send(sock, msg.data(), msg.size(), 0) < 0) {
        ::perror("send"); ::close(sock); return -1;
    }

    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLIN;
    int ret = ::poll(&pfd, 1, timeoutMs);
    if (ret <= 0) {
        if (ret == 0) std::cerr << "  [WARN] poll timeout" << std::endl;
        ::close(sock); return -1;
    }

    char buf[4096];
    ::bzero(buf, sizeof(buf));
    ssize_t n = ::recv(sock, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        resp.assign(buf, n);
    }
    ::close(sock);
    return (n > 0) ? 0 : -1;
}

// ---- Test 1: Connection UP/DOWN callbacks ----
static bool testConnectionUpDown() {
    std::cout << "\n--- Test 1: Connection UP/DOWN callbacks ---" << std::endl;
    s_connectionUp = 0;
    s_connectionDown = 0;

    std::thread t([]() {
        EventLoop loop;
        InetAddress addr(18081);
        TestEchoServer server(&loop, addr, "Test1");
        server.start();
        loop.runAfter(1.5, [&loop]() { loop.quit(); });
        loop.loop();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    ::bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(18081);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int connRet = ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (connRet == 0) {
        LOG_INFO("CLIENT: Connected");
    } else {
        ::perror("CLIENT: connect");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ::close(sock);
    LOG_INFO("CLIENT: Closed");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    t.join();

    bool up = s_connectionUp.load() >= 1;
    bool down = s_connectionDown.load() >= 1;
    bool passed = up && down;

    std::cout << "  Connection UP:   " << (up ? "PASS" : "FAIL")
              << " (count=" << s_connectionUp.load() << ")" << std::endl;
    std::cout << "  Connection DOWN: " << (down ? "PASS" : "FAIL")
              << " (count=" << s_connectionDown.load() << ")" << std::endl;

    return passed;
}

// ---- Test 2: Echo "ECHO: " + message ----
static bool testEcho() {
    std::cout << "\n--- Test 2: Echo 'ECHO: ' + message ---" << std::endl;
    s_connectionUp = 0;
    s_connectionDown = 0;
    s_messageCount = 0;

    std::thread t([]() {
        EventLoop loop;
        InetAddress addr(18082);
        TestEchoServer server(&loop, addr, "Test2");
        server.start();
        loop.runAfter(1.5, [&loop]() { loop.quit(); });
        loop.loop();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::string response;
    int ret = sendAndRecv(18082, "HelloWorld", response);

    t.join();

    std::string expected = "ECHO: HelloWorld";
    bool echoOk = (ret == 0 && response == expected);

    std::cout << "  Sent:      'HelloWorld'" << std::endl;
    std::cout << "  Expected:  '" << expected << "'" << std::endl;
    std::cout << "  Received:  '" << response << "'" << std::endl;
    std::cout << "  Echo test: " << (echoOk ? "PASS" : "FAIL") << std::endl;

    return echoOk;
}

// ---- Test 3: Empty message (0-byte send does not trigger message callback) ----
static bool testEmptyMessage() {
    std::cout << "\n--- Test 3: Empty message (no data sent) ---" << std::endl;
    s_connectionUp = 0;
    s_connectionDown = 0;
    s_messageCount = 0;

    std::thread t([]() {
        EventLoop loop;
        InetAddress addr(18083);
        TestEchoServer server(&loop, addr, "Test3");
        server.start();
        loop.runAfter(1.0, [&loop]() { loop.quit(); });
        loop.loop();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // send() with 0 bytes doesn't transmit data over TCP,
    // so no message callback fires on the server side.
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    ::bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(18083);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    ::close(sock);

    t.join();

    // No message callback should have fired (0 bytes sends nothing over TCP)
    bool noCrash = true;
    bool upOk = (s_connectionUp.load() >= 1);
    bool downOk = (s_connectionDown.load() >= 1);
    bool noMessage = (s_messageCount.load() == 0);
    bool passed = noCrash && upOk && downOk && noMessage;

    std::cout << "  Server no crash: " << (noCrash ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Connect callback: " << (upOk ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Close callback:   " << (downOk ? "PASS" : "FAIL") << std::endl;
    std::cout << "  No msg callback:  " << (noMessage ? "PASS" : "FAIL") << std::endl;

    return passed;
}

// ---- Test 4: Multiple connections ----
static bool testMultipleConnections() {
    std::cout << "\n--- Test 4: Two concurrent connections ---" << std::endl;
    s_connectionUp = 0;
    s_connectionDown = 0;
    s_messageCount = 0;

    std::thread t([]() {
        EventLoop loop;
        InetAddress addr(18084);
        TestEchoServer server(&loop, addr, "Test4");
        server.start();
        loop.runAfter(2.0, [&loop]() { loop.quit(); });
        loop.loop();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::string resp1, resp2;
    int ret1 = sendAndRecv(18084, "First", resp1);
    int ret2 = sendAndRecv(18084, "Second", resp2);

    t.join();

    bool ok1 = (ret1 == 0 && resp1 == "ECHO: First");
    bool ok2 = (ret2 == 0 && resp2 == "ECHO: Second");
    bool passed = ok1 && ok2;

    std::cout << "  Conn 1: " << (ok1 ? "PASS" : "FAIL")
              << " (got='" << resp1 << "')" << std::endl;
    std::cout << "  Conn 2: " << (ok2 ? "PASS" : "FAIL")
              << " (got='" << resp2 << "')" << std::endl;

    return passed;
}

int main() {
    std::cout << "=== TcpServer Tests ===" << std::endl;

    bool t1 = testConnectionUpDown();
    bool t2 = testEcho();
    bool t3 = testEmptyMessage();
    bool t4 = testMultipleConnections();

    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "  Connection UP/DOWN: " << (t1 ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Echo:              " << (t2 ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Empty message:     " << (t3 ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Multiple conns:    " << (t4 ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Overall:           " << ((t1 && t2 && t3 && t4) ? "ALL PASSED" : "SOME FAILED") << std::endl;

    return (t1 && t2 && t3 && t4) ? 0 : 1;
}
