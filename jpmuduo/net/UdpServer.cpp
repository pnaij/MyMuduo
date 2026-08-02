//
// Created by jianp on 2026/5/31.
//

#include "jpmuduo/net/UdpServer.h"
#include "jpmuduo/base/Logger.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/Buffer.h"

#include <sys/socket.h>
#include <errno.h>

namespace jpmuduo {

static EventLoop* CheckLoopNotNull(EventLoop* loop) {
    if (loop == nullptr) {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

UdpServer::UdpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& name)
    : loop_(CheckLoopNotNull(loop))
    , name_(name)
    , socket_(new Socket(Socket::createUdpSocket()))
    , channel_(new Channel(loop, socket_->fd()))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , started_(false)
{
    socket_->setReuseAddr(true);
    socket_->bindAddress(listenAddr);

    channel_->setReadCallback(std::bind(&UdpServer::handleRead, this, std::placeholders::_1));
    channel_->setErrorCallback([this]() {
        LOG_ERROR("UdpServer[%s] fd=%d socket error\n", name_.c_str(), socket_->fd());
    });

    LOG_INFO("UdpServer[%s] created, fd=%d\n", name_.c_str(), socket_->fd());
}

UdpServer::~UdpServer() {
    if (started_.load()) {
        channel_->disableAll();
        channel_->remove();
    }
}

void UdpServer::start() {
    if (started_.exchange(true)) return;

    threadPool_->start();
    loop_->runInLoop([this]() {
        channel_->enableReading();
        LOG_INFO("UdpServer[%s] started on fd=%d\n", name_.c_str(), socket_->fd());
    });
}

void UdpServer::setThreadNum(int numThreads) {
    threadPool_->setThreadNum(numThreads);
}

void UdpServer::sendTo(const void* data, size_t len, const InetAddress& peerAddr) {
    if (loop_->isInLoopThread()) {
        ssize_t n = socket_->sendto(data, len, peerAddr);
        if (n < 0) {
            LOG_ERROR("UdpServer[%s] sendto error: %d\n", name_.c_str(), errno);
        }
    } else {
        loop_->runInLoop([this, copy = std::string(static_cast<const char*>(data), len), peerAddr]() {
            ssize_t n = socket_->sendto(copy.data(), copy.size(), peerAddr);
            if (n < 0) {
                LOG_ERROR("UdpServer[%s] sendto error: %d\n", name_.c_str(), errno);
            }
        });
    }
}

void UdpServer::sendTo(const std::string& data, const InetAddress& peerAddr) {
    sendTo(data.data(), data.size(), peerAddr);
}

void UdpServer::handleRead(TimeStamp receiveTime) {
    EventLoop* workerLoop = threadPool_->getNextLoop();
    char buf[65536];

    InetAddress senderAddr;
    ssize_t n = socket_->recvfrom(buf, sizeof(buf), &senderAddr);
    if (n < 0) {
        LOG_ERROR("UdpServer[%s] recvfrom error: %d\n", name_.c_str(), errno);
        return;
    }

    if (workerLoop == loop_) {
        Buffer buffer;
        buffer.append(buf, n);
        if (messageCallback_) {
            messageCallback_(&buffer, receiveTime, senderAddr);
        }
    } else {
        workerLoop->queueInLoop([this, data = std::string(buf, n), receiveTime, senderAddr]() {
            Buffer buffer;
            buffer.append(data.data(), data.size());
            if (messageCallback_) {
                messageCallback_(&buffer, receiveTime, senderAddr);
            }
        });
    }
}

}  // namespace jpmuduo
