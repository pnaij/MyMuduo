//
// Created by jianp on 2025/12/11.
//

#ifndef MUDUOSELF_TCPSERVER_H
#define MUDUOSELF_TCPSERVER_H

#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

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

    void setThreadNum(int numThreads);

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

    ThreadinitCallback threadinitCallback_;

    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_;
};


#endif //MUDUOSELF_TCPSERVER_H
