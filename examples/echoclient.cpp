//
// echoclient.cpp - interactive echo client using TcpClient
// Usage: ./echoclient [host] [port]
//

#include "jpmuduo/net/TcpClient.h"
#include "jpmuduo/net/TcpConnection.h"
#include "jpmuduo/net/Buffer.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/EventLoopThread.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/base/Logging.h"

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace jpmuduo;

class EchoClient {
public:
    EchoClient(EventLoop* loop, const InetAddress& serverAddr)
        : loop_(loop)
        , client_(loop, serverAddr, "EchoClient")
        , connected_(false) {
        client_.setConnectionCallback(
            [this](const TcpConnectionPtr& conn) {
                if (conn->connected()) {
                    LOG_INFO << "Connected to " << conn->peerAddress().toIpPort();
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        connection_ = conn;
                        connected_ = true;
                    }
                    connReady_.notify_one();
                } else {
                    LOG_INFO << "Disconnected";
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        connection_.reset();
                        connected_ = false;
                    }
                    loop_->quit();
                }
            });
        client_.setMessageCallback(
            [this](const TcpConnectionPtr&, Buffer* buf, TimeStamp) {
                std::cout << buf->retrieveAllAsString() << std::endl;
            });
    }

    void connect()  { client_.connect(); }
    void disconnect() { client_.disconnect(); }

    void waitConnected() {
        std::unique_lock<std::mutex> lock(mutex_);
        connReady_.wait(lock, [this] { return connected_; });
    }

    void send(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connection_) {
            connection_->send(msg);
        }
    }

    EventLoop* getLoop() { return loop_; }

private:
    EventLoop* loop_;
    TcpClient client_;
    TcpConnectionPtr connection_;
    bool connected_;
    std::mutex mutex_;
    std::condition_variable connReady_;
};

int main(int argc, char* argv[]) {
    const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
    uint16_t    port = (argc > 2) ? static_cast<uint16_t>(std::stoi(argv[2])) : 8888;

    // EventLoop 线程绑定：EventLoopThread 在子线程创建并运行 loop，
    // 主线程通过 runInLoop 跨线程投递 connect（符合 muduo 线程模型）
    EventLoopThread loopThread;
    EventLoop* loop = loopThread.startLoop();
    InetAddress addr(port, host);
    EchoClient client(loop, addr);

    client.connect();

    client.waitConnected();
    std::cout << "Type message (Ctrl+D or /quit to exit):" << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "/quit") break;
        client.send(line);
    }

    client.disconnect();
    // loopThread 析构时 quit + join，主线程等待 IO 线程收尾
    return 0;
}
