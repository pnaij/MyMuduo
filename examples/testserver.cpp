//
// Created by jianp on 2025/12/11.
//

#include "jpmuduo/net/TcpServer.h"
#include "jpmuduo/base/Logging.h"
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

        // thread pool auto-sizes from std::thread::hardware_concurrency()
    }

    void start() {
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr& conn) {
        if(conn->connected()) {
            LOG_INFO << "Connection UP : " << conn->peerAddress().toIpPort();
        }else {
            LOG_INFO << "Connection DOWN : " << conn->peerAddress().toIpPort();
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, TimeStamp time) {
        // ── 粘包/拆包处理: 循环解码长度前缀消息 ──
        while (buf->readableBytes() >= sizeof(int32_t)) {
            int32_t len = buf->peekInt32();
            if (len < 0 || len > 64 * 1024 * 1024) {
                LOG_ERROR << "Invalid length: " << len;
                conn->shutdown();
                return;
            }
            if (buf->readableBytes() < sizeof(int32_t) + static_cast<size_t>(len)) {
                break;  // 半包，等下次数据
            }
            buf->retrieve(sizeof(int32_t));
            std::string msg = buf->retrieveAsString(len);

            // Encoded echo: prepend length header before send
            Buffer out;
            out.append(msg.data(), msg.size());
            out.prependInt32(static_cast<int32_t>(msg.size()));
            conn->send(std::string(out.peek(), out.readableBytes()));
        }
        conn->shutdown();
    }

    EventLoop* loop_;
    TcpServer server_;
};

int main() {
    EventLoop loop;         //baseLoop
    InetAddress addr(8000);
    EchoServer server(&loop, addr, "EchoServer-01");

    server.start();
    loop.loop();

    return 0;
}
