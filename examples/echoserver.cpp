//
// echoserver.cpp - raw text echo: nc → send anything → get it back
//

#include "jpmuduo/net/TcpServer.h"
#include "jpmuduo/base/Logger.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/net/Buffer.h"

using namespace jpmuduo;

class EchoServer {
public:
    EchoServer(EventLoop* loop, const InetAddress& addr)
        : server_(loop, addr, "EchoServer") {
        server_.setConnectionCallback(
            [](const TcpConnectionPtr& conn) {
                if (conn->connected()) {
                    LOG_INFO("UP   : %s", conn->peerAddress().toIpPort().c_str());
                } else {
                    LOG_INFO("DOWN : %s", conn->peerAddress().toIpPort().c_str());
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
    EventLoop loop;
    InetAddress addr(8888);
    EchoServer server(&loop, addr);
    server.start();

    LOG_INFO("EchoServer listening on port 8888");
    loop.loop();
    return 0;
}
