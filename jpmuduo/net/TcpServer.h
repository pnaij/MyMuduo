//
// Created by jianp on 2025/12/11.
//

#ifndef JPMUDUO_TCPSERVER_H
#define JPMUDUO_TCPSERVER_H

#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/Acceptor.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/base/noncopyable.h"
#include "jpmuduo/net/EventLoopThreadPool.h"
#include "jpmuduo/net/Callbacks.h"
#include "jpmuduo/net/TcpConnection.h"
#include "jpmuduo/net/Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

namespace jpmuduo {

class TcpServer : noncopyable {
public:
    using ThreadinitCallback = std::function<void(EventLoop*)>;

    enum Option {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option = kNoReusePort);
    ~TcpServer();


    void setThreadInitCallback(const ThreadinitCallback& cb) { threadinitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    void setThreadNum(int numThreads);

    int64_t currentConnections() const { return currentConnections_.load(); }
    int64_t totalConnections() const { return totalConnections_.load(); }

    void start();
private:
    void newConnection(int sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    EventLoop* loop_;

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;
    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    // 默认与 TcpConnection 构造一致（64MB），未调用 setter 时也有合理值
    size_t highWaterMark_ = 64 * 1024 * 1024;

    ThreadinitCallback threadinitCallback_;

    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_;

    std::atomic<int64_t> currentConnections_{0};
    std::atomic<int64_t> totalConnections_{0};
};


}  // namespace jpmuduo

#endif //JPMUDUO_TCPSERVER_H
