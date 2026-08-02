// Benchmark echo server - keeps connections alive
#include "jpmuduo/net/TcpServer.h"
#include "jpmuduo/base/Logger.h"
#include "jpmuduo/net/Buffer.h"

#include <string>
#include <functional>

using namespace jpmuduo;

class EchoServer {
public:
    EchoServer(EventLoop* loop,
               const InetAddress& addr,
               const std::string& name) : server_(loop, addr, name), loop_(loop) {
        server_.setConnectionCallback(
                std::bind(&EchoServer::onConnection, this, std::placeholders::_1));

        server_.setMessageCallback(
                std::bind(&EchoServer::onMessage, this,
                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        server_.setHighWaterMarkCallback(
                std::bind(&EchoServer::onHighWaterMark, this,
                          std::placeholders::_1, std::placeholders::_2));

        server_.setWriteCompleteCallback(
                std::bind(&EchoServer::onWriteComplete, this,
                          std::placeholders::_1));

        // thread pool auto-sizes from std::thread::hardware_concurrency()
    }

    void start() { server_.start(); }

private:
    void onConnection(const TcpConnectionPtr& conn) {
        if(conn->connected()) {
            LOG_INFO("Connection UP : %s", conn->peerAddress().toIpPort().c_str());
        } else {
            LOG_INFO("Connection DOWN : %s", conn->peerAddress().toIpPort().c_str());
        }
    }

    void onHighWaterMark(const TcpConnectionPtr& conn, size_t bytes) {
        LOG_INFO("High water mark %zu bytes, stop reading: %s", bytes, conn->peerAddress().toIpPort().c_str());
        conn->stopRead();
    }

    void onWriteComplete(const TcpConnectionPtr& conn) {
        conn->startRead();
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, TimeStamp time) {
        // Zero-copy echo: [4B header][payload] already formatted, send as-is
        while (buf->readableBytes() >= sizeof(int32_t)) {
            int32_t len = buf->peekInt32();
            if (len < 0 || len > 64 * 1024 * 1024) {
                LOG_ERROR("Invalid length: %d\n", len);
                conn->shutdown();
                return;
            }
            if (buf->readableBytes() < sizeof(int32_t) + static_cast<size_t>(len)) {
                break;
            }
            conn->send(buf->peek(), sizeof(int32_t) + len);
            buf->retrieve(sizeof(int32_t) + len);
        }
    }

    EventLoop* loop_;
    TcpServer server_;
};

int main() {
    EventLoop loop;
    InetAddress addr(8000);
    EchoServer server(&loop, addr, "EchoServer-bench");

    server.start();
    loop.loop();

    return 0;
}
