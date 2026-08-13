//
// echoserver.cpp - raw text echo: nc → send anything → get it back
//

#include "jpmuduo/net/TcpServer.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/net/Buffer.h"
#include "jpmuduo/base/AsyncLogging.h"
#include "jpmuduo/base/Logging.h"

#include <signal.h>

using namespace jpmuduo;

EventLoop* g_loop = nullptr;  // 供信号处理器唤醒主循环优雅退出

void quitHandler(int) {
    if (g_loop) {
        g_loop->quit();
    }
}

class EchoServer {
public:
    EchoServer(EventLoop* loop, const InetAddress& addr)
        : server_(loop, addr, "EchoServer") {
        server_.setConnectionCallback(
            [](const TcpConnectionPtr& conn) {
                if (conn->connected()) {
                    LOG_INFO << "UP   : " << conn->peerAddress().toIpPort();
                } else {
                    LOG_INFO << "DOWN : " << conn->peerAddress().toIpPort();
                }
            });
        server_.setMessageCallback(
            [](const TcpConnectionPtr& conn, Buffer* buf, TimeStamp) {
                std::string msg = buf->retrieveAllAsString();
                conn->send(msg);     // raw echo
            });
    }

    void start() { server_.start(); }

private:
    TcpServer server_;
};

int main() {
    // 日志落盘：所有 LOG_* 的输出路由到 AsyncLogging → 当前目录 echoserver.*.log
    AsyncLogging log("echoserver", 500*1000*1000);  // 500MB 滚动
    log.start();
    Logger::setOutput(std::bind(&AsyncLogging::append, &log,
                                std::placeholders::_1, std::placeholders::_2));

    ::signal(SIGINT, quitHandler);
    ::signal(SIGTERM, quitHandler);

    EventLoop loop;
    g_loop = &loop;
    InetAddress addr(8888);
    EchoServer server(&loop, addr);
    server.start();

    LOG_INFO << "EchoServer listening on port 8888";
    loop.loop();
    return 0;
}
