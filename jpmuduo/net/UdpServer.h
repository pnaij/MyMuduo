//
// Created by jianp on 2026/5/31.
//

#ifndef JPMUDUO_UDPSERVER_H
#define JPMUDUO_UDPSERVER_H

#include "jpmuduo/base/noncopyable.h"
#include "jpmuduo/net/Callbacks.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/net/Socket.h"
#include "jpmuduo/net/Channel.h"
#include "jpmuduo/net/EventLoopThreadPool.h"

#include <atomic>
#include <memory>
#include <string>

namespace jpmuduo {

class EventLoop;
class Buffer;

class UdpServer : noncopyable {
public:
    UdpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& name);
    ~UdpServer();

    void start();
    void setThreadNum(int numThreads);

    void sendTo(const void* data, size_t len, const InetAddress& peerAddr);
    void sendTo(const std::string& data, const InetAddress& peerAddr);

    void setMessageCallback(const UdpServerMessageCallback& cb) { messageCallback_ = cb; }

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }

private:
    void handleRead(TimeStamp receiveTime);

    EventLoop* loop_;
    const std::string name_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    std::shared_ptr<EventLoopThreadPool> threadPool_;

    UdpServerMessageCallback messageCallback_;

    std::atomic_bool started_;
};

}  // namespace jpmuduo

#endif // JPMUDUO_UDPSERVER_H
