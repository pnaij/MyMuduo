//
// Created by jianp on 2025/12/11.
//

#ifndef JPMUDUO_TCPCONNECTION_H
#define JPMUDUO_TCPCONNECTION_H

#include "jpmuduo/base/noncopyable.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/net/Callbacks.h"
#include "jpmuduo/net/Buffer.h"
#include "jpmuduo/base/TimeStamp.h"

#include <memory>
#include <string>
#include <atomic>

namespace jpmuduo {

class Buffer;
class Channel;
class EventLoop;
class Socket;

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(EventLoop* loop,
                  const std::string& nameArg,
                  int sockfd,
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    void send(const std::string& buf);
    void send(Buffer* buf);
    void send(const void* data, int len);
    void send(const void* data, size_t len);
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
        writeCompleteCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb) {
        highWaterMarkCallback_ = cb;
    }

    void setCloseCallback(const CloseCallback& cb) {
        closeCallback_ = cb;
    }

    void startRead();
    void stopRead();
    void setKeepAlive(bool on);

    void connectEstablished();
    void connectDestroyed();
private:
    enum StateE {
        kDisconnected, kConnecting, kConnected, kDisconnecting
    };
    void setState(StateE state) { state_ = state; }

    void handleRead(TimeStamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void sendInLoop(Buffer* buf);
    void shutdownInLoop();

    EventLoop* loop_;
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;
};


}  // namespace jpmuduo

#endif //JPMUDUO_TCPCONNECTION_H
