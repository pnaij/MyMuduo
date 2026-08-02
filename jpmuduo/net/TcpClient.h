//
// Created by jianp on 2026/5/16.
//

#ifndef JPMUDUO_TCPCLIENT_H
#define JPMUDUO_TCPCLIENT_H

#include "jpmuduo/base/noncopyable.h"
#include "jpmuduo/net/Callbacks.h"
#include "jpmuduo/net/InetAddress.h"

#include <memory>
#include <string>
#include <mutex>

namespace jpmuduo {

class EventLoop;
class Connector;
class TcpConnection;

class TcpClient : noncopyable {
public:
    TcpClient(EventLoop* loop, const InetAddress& serverAddr, const std::string& name);
    ~TcpClient();

    void connect();
    void disconnect();
    void stop();

    bool retry() const { return retry_; }
    void enableRetry() { retry_ = true; }

    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb) { highWaterMarkCallback_ = cb; }

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }

private:
    void newConnection(int sockfd);
    void removeConnection(const TcpConnectionPtr& conn);

    EventLoop* loop_;
    std::shared_ptr<Connector> connector_;
    const std::string name_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    bool retry_;
    int nextConnId_;
    mutable std::mutex mutex_;
    TcpConnectionPtr connection_;
};

}  // namespace jpmuduo

#endif // JPMUDUO_TCPCLIENT_H
