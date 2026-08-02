//
// Created by jianp on 2026/5/31.
//

#ifndef JPMUDUO_UDPCLIENT_H
#define JPMUDUO_UDPCLIENT_H

#include "jpmuduo/base/noncopyable.h"
#include "jpmuduo/net/Callbacks.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/net/Socket.h"
#include "jpmuduo/net/Channel.h"

#include <atomic>
#include <memory>
#include <string>

namespace jpmuduo {

class EventLoop;

class UdpClient : noncopyable {
public:
    UdpClient(EventLoop* loop, const InetAddress& serverAddr, const std::string& name);
    ~UdpClient();

    void start();
    void connect();
    void disconnect();
    void send(const std::string& data);
    void send(const void* data, size_t len);
    void sendTo(const void* data, size_t len, const InetAddress& peerAddr);
    void sendTo(const std::string& data, const InetAddress& peerAddr);

    void setMessageCallback(const UdpServerMessageCallback& cb) { messageCallback_ = cb; }

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    bool connected() const { return connected_.load(); }

private:
    void handleRead(TimeStamp receiveTime);
    void handleError();

    EventLoop* loop_;
    const std::string name_;
    InetAddress serverAddr_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    UdpServerMessageCallback messageCallback_;
    std::atomic_bool connected_;
};

}  // namespace jpmuduo

#endif // JPMUDUO_UDPCLIENT_H
