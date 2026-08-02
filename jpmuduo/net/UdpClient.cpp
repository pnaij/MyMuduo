//
// Created by jianp on 2026/5/31.
//

#include "jpmuduo/net/UdpClient.h"
#include "jpmuduo/base/Logger.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/Buffer.h"

#include <sys/socket.h>
#include <errno.h>

namespace jpmuduo {

static EventLoop* CheckLoopNotNull(EventLoop* loop) {
    if (loop == nullptr) {
        LOG_FATAL("%s:%s:%d loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

UdpClient::UdpClient(EventLoop* loop, const InetAddress& serverAddr, const std::string& name)
    : loop_(CheckLoopNotNull(loop))
    , name_(name)
    , serverAddr_(serverAddr)
    , socket_(new Socket(Socket::createUdpSocket()))
    , channel_(new Channel(loop, socket_->fd()))
    , connected_(false)
{
    socket_->setReuseAddr(true);

    channel_->setReadCallback(std::bind(&UdpClient::handleRead, this, std::placeholders::_1));
    channel_->setErrorCallback(std::bind(&UdpClient::handleError, this));

    LOG_INFO("UdpClient[%s] created, fd=%d\n", name_.c_str(), socket_->fd());
}

UdpClient::~UdpClient() {
    if (connected_.load()) {
        disconnect();
    }
}

void UdpClient::start() {
    loop_->runInLoop([this]() {
        channel_->enableReading();
        LOG_INFO("UdpClient[%s] started on fd=%d\n", name_.c_str(), socket_->fd());
    });
}

void UdpClient::connect() {
    if (connected_.exchange(true)) return;

    loop_->runInLoop([this]() {
        sockaddr_in addr = *serverAddr_.getSockAddr();
        int ret = ::connect(socket_->fd(), (const sockaddr*)&addr, sizeof(addr));
        if (ret < 0) {
            LOG_ERROR("UdpClient[%s] connect error: %d\n", name_.c_str(), errno);
            connected_ = false;
            return;
        }
        channel_->enableReading();
        LOG_INFO("UdpClient[%s] connected to %s, fd=%d\n",
                 name_.c_str(), serverAddr_.toIpPort().c_str(), socket_->fd());
    });
}

void UdpClient::disconnect() {
    if (!connected_.exchange(false)) return;

    loop_->runInLoop([this]() {
        channel_->disableAll();
        channel_->remove();
        LOG_INFO("UdpClient[%s] disconnected, fd=%d\n", name_.c_str(), socket_->fd());
    });
}

void UdpClient::send(const void* data, size_t len) {
    loop_->runInLoop([this, copy = std::string(static_cast<const char*>(data), len)]() {
        ssize_t n = ::send(socket_->fd(), copy.data(), copy.size(), 0);
        if (n < 0) {
            LOG_ERROR("UdpClient[%s] send error: %d\n", name_.c_str(), errno);
        }
    });
}

void UdpClient::send(const std::string& data) {
    send(data.data(), data.size());
}

void UdpClient::sendTo(const void* data, size_t len, const InetAddress& peerAddr) {
    loop_->runInLoop([this, copy = std::string(static_cast<const char*>(data), len), peerAddr]() {
        ssize_t n = socket_->sendto(copy.data(), copy.size(), peerAddr);
        if (n < 0) {
            LOG_ERROR("UdpClient[%s] sendTo error: %d\n", name_.c_str(), errno);
        }
    });
}

void UdpClient::sendTo(const std::string& data, const InetAddress& peerAddr) {
    sendTo(data.data(), data.size(), peerAddr);
}

void UdpClient::handleRead(TimeStamp receiveTime) {
    char buf[65536];
    InetAddress senderAddr;
    ssize_t n = socket_->recvfrom(buf, sizeof(buf), &senderAddr);
    if (n < 0) {
        LOG_ERROR("UdpClient[%s] recvfrom error: %d\n", name_.c_str(), errno);
        return;
    }

    if (messageCallback_) {
        Buffer buffer;
        buffer.append(buf, n);
        messageCallback_(&buffer, receiveTime, senderAddr);
    }
}

void UdpClient::handleError() {
    LOG_ERROR("UdpClient[%s] fd=%d error\n", name_.c_str(), socket_->fd());
}

}  // namespace jpmuduo
