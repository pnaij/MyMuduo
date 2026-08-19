//
// Created by jianp on 2025/12/11.
//

#ifndef JPMUDUO_TCPCONNECTION_H
#define JPMUDUO_TCPCONNECTION_H

#include "jpmuduo/base/noncopyable.h"
#include "jpmuduo/base/WeakCallback.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/net/Callbacks.h"
#include "jpmuduo/net/Buffer.h"
#include "jpmuduo/base/TimeStamp.h"

#include <any>
#include <memory>
#include <string>
#include <atomic>
#include <netinet/tcp.h>  // struct tcp_info

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

    // return true if success.
    bool getTcpInfo(struct tcp_info*) const;
    std::string getTcpInfoString() const;

    void send(const std::string& buf);
    void send(Buffer* buf);
    void send(const void* data, int len);
    void send(const void* data, size_t len);
    void shutdown();
    // 优雅关闭（等数据发完再断）；forceClose 立即走 handleClose 全关
    void forceClose();
    void forceCloseWithDelay(double seconds);

    // 用户数据附着：业务层可把任意对象挂到连接上
    void setContext(const std::any& context) { context_ = context; }
    const std::any& getContext() const { return context_; }
    std::any* getMutableContext() { return &context_; }

    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
        writeCompleteCallback_ = cb;
    }

    // 高水位回调 + 水位一起设置（原始 muduo 签名）
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
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
    const char* stateToString() const;

    void handleRead(TimeStamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void sendInLoop(Buffer* buf);
    void shutdownInLoop();
    void forceCloseInLoop();

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

    std::any context_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;
};


}  // namespace jpmuduo

#endif //JPMUDUO_TCPCONNECTION_H
