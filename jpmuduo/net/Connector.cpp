//
// Created by jianp on 2026/5/16.
//

#include "jpmuduo/net/Connector.h"
#include "jpmuduo/net/Channel.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/base/Logging.h"
#include "jpmuduo/net/Socket.h"
#include "jpmuduo/net/SocketsOps.h"

#include <errno.h>

namespace jpmuduo {

const int Connector::kMaxRetryDelayMs;
const int Connector::kInitRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop)
    , serverAddr_(serverAddr)
    , connect_(false)
    , state_(kDisconnected)
    , retryDelayMs_(kInitRetryDelayMs)
{}

Connector::~Connector() = default;

void Connector::start() {
    connect_ = true;
    loop_->runInLoop([this]() { startInLoop(); });
}

void Connector::startInLoop() {
    if (state_ != kDisconnected) return;
    connect();
}

void Connector::restart() {
    loop_->runInLoop([this]() {
        setState(kDisconnected);
        retryDelayMs_ = kInitRetryDelayMs;
        connect_ = true;
        startInLoop();
    });
}

void Connector::stop() {
    connect_ = false;
    loop_->runInLoop([this]() { stopInLoop(); });
}

void Connector::stopInLoop() {
    if (state_ == kConnecting) {
        setState(kDisconnected);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

void Connector::connect() {
    int sockfd = Socket::createTcpSocket();
    int ret = sockets::connect(sockfd, sockets::sockaddr_cast(serverAddr_.getSockAddr()));
    int savedErrno = (ret == 0) ? 0 : errno;

    switch (savedErrno) {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
        connecting(sockfd);
        break;
    default:
        LOG_ERROR << "Connector::connect error:" << savedErrno;
        sockets::close(sockfd);
        retry(sockfd);
        break;
    }
}

void Connector::connecting(int sockfd) {
    setState(kConnecting);
    channel_.reset(new Channel(loop_, sockfd));
    channel_->setWriteCallback(std::bind(&Connector::handleWrite, this));
    channel_->setErrorCallback(std::bind(&Connector::handleError, this));
    channel_->enableWriting();
}

void Connector::handleWrite() {
    if (state_ == kConnecting) {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);

        if (err) {
            LOG_ERROR << "Connector::handleWrite - SO_ERROR: " << err;
            retry(sockfd);
        } else if (sockets::isSelfConnect(sockfd)) {
            LOG_ERROR << "Connector::handleWrite - self connect, retry";
            sockets::close(sockfd);
            retry(sockfd);
        } else {
            setState(kConnected);
            if (connect_ && newConnectionCallback_) {
                newConnectionCallback_(sockfd);
            } else {
                sockets::close(sockfd);
            }
        }
    }
}

void Connector::handleError() {
    LOG_ERROR << "Connector::handleError state=" << state_;
    if (state_ == kConnecting) {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        LOG_ERROR << "SO_ERROR=" << err;
        retry(sockfd);
    }
}

void Connector::retry(int sockfd) {
    sockets::close(sockfd);
    setState(kDisconnected);

    if (connect_) {
        LOG_INFO << "Connector::retry - retry in " << retryDelayMs_ << " ms";
        std::shared_ptr<Connector> self(shared_from_this());
        loop_->runAfter(retryDelayMs_ / 1000.0, [self]() {
            self->startInLoop();
        });
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
    }
}

int Connector::removeAndResetChannel() {
    channel_->disableAll();
    channel_->remove();
    int sockfd = channel_->fd();
    loop_->queueInLoop([this]() { resetChannel(); });
    return sockfd;
}

void Connector::resetChannel() {
    channel_.reset();
}

}  // namespace jpmuduo
