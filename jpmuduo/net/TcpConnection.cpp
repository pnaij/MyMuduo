//
// Created by jianp on 2025/12/11.
//

#include "jpmuduo/net/TcpConnection.h"
#include "jpmuduo/base/Logging.h"
#include "jpmuduo/net/Socket.h"
#include "jpmuduo/net/Channel.h"
#include "jpmuduo/net/EventLoop.h"

#include <functional>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <string>

namespace jpmuduo {

static EventLoop* CheckLoopNotNull(EventLoop* loop) {
    if(loop == nullptr) {
        LOG_FATAL << "TcpConnection Loop is null!";
    }

    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg, int sockfd, const InetAddress &localAddr,
                             const InetAddress &peerAddr)
                             : loop_(CheckLoopNotNull(loop))
                             , name_(nameArg)
                             , state_(kConnecting)
                             , reading_(true)
                             , socket_(new Socket(sockfd))
                             , channel_(new Channel(loop, sockfd))
                             , localAddr_(localAddr)
                             , peerAddr_(peerAddr)
                             , highWaterMark_(64 * 1024 * 1024) {//高水位线64MB
    channel_->setReadCallback(
            std::bind(&TcpConnection::handleRead, this, std::placeholders::_1)
            );
    channel_->setWriteCallback(
            std::bind(&TcpConnection::handleWrite, this)
            );
    channel_->setCloseCallback(
            std::bind(&TcpConnection::handleClose, this)
            );
    channel_->setErrorCallback(
            std::bind(&TcpConnection::handleError, this)
            );

    LOG_INFO << "TcpConnection::ctor[" << name_ << "] at fd=" << sockfd;
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
    LOG_INFO << "TcpConnection::dtor[" << name_ << "] at fd=" << channel_->fd()
             << " state=" << static_cast<int>(state_);
}

void TcpConnection::send(const std::string &buf) {
    if(state_ == kConnected) {
        if(loop_->isInLoopThread()) {
            sendInLoop(buf.c_str(), buf.size());
        }else {
            // copy buf to avoid dangling pointer on cross-thread call
            loop_->runInLoop([this, copy = std::string(buf)]() {
                sendInLoop(copy.data(), copy.size());
            });
        }
    }
}

void TcpConnection::send(Buffer* buf) {
    if(state_ == kConnected) {
        if(loop_->isInLoopThread()) {
            sendInLoop(buf);
        }else {
            loop_->runInLoop([this, buf]() mutable {
                sendInLoop(buf);
            });
        }
    }
}

void TcpConnection::send(const void* data, int len) {
    send(data, static_cast<size_t>(len));
}

void TcpConnection::send(const void* data, size_t len) {
    if(state_ == kConnected) {
        if(loop_->isInLoopThread()) {
            sendInLoop(data, len);
        }else {
            loop_->runInLoop([this, copy = std::string(static_cast<const char*>(data), len)]() {
                sendInLoop(copy.data(), copy.size());
            });
        }
    }
}

void TcpConnection::sendInLoop(const void *message, size_t len) {
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if(state_ == kDisconnecting) {
        LOG_ERROR << "disconnected, give up writing!";
        return ;
    }

    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), message, len);
        if(nwrote >= 0) {
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_) {
                loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())
                        );
            }
        }else {
            nwrote = 0;
            if(errno != EWOULDBLOCK) {
                LOG_ERROR << "TcpConnection::sendInLoop";
                if(errno == EPIPE || errno == ECONNRESET) {
                     faultError = true;
                }
            }
        }
    }

    if(!faultError && remaining > 0) {
        size_t oldLen = outputBuffer_.readableBytes();
        if(oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_) {
            loop_->queueInLoop(
                    std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining)
                    );
        }

        outputBuffer_.append((char*)message + nwrote, remaining);
        if(!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdown() {
    if(state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(
                std::bind(&TcpConnection::shutdownInLoop, this)
                );
    }
}

void TcpConnection::sendInLoop(Buffer* buf) {
    sendInLoop(buf->peek(), buf->readableBytes());
}

void TcpConnection::shutdownInLoop() {
    if(!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}

void TcpConnection::setKeepAlive(bool on) {
    socket_->setKeepAlive(on);
}

void TcpConnection::startRead() {
    loop_->runInLoop([this]() {
        if (!reading_) {
            reading_ = true;
            channel_->enableReading();
        }
    });
}

void TcpConnection::stopRead() {
    loop_->runInLoop([this]() {
        if (reading_) {
            reading_ = false;
            channel_->disableReading();
        }
    });
}

void TcpConnection::connectEstablished() {
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    if (connectionCallback_) {
        connectionCallback_(shared_from_this());
    }
}

void TcpConnection::connectDestroyed() {
    if(state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnected);
        channel_->disableAll();
        if (connectionCallback_) {
            connectionCallback_(shared_from_this());
        }
    }

    channel_->remove();
}

void TcpConnection::handleRead(TimeStamp receiveTime) {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if(n > 0) {
        if (messageCallback_) {
            messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
        } else {
            inputBuffer_.retrieveAll();
        }
    }else if(n == 0) {
        handleClose();
    }else {
        errno = savedErrno;
        LOG_ERROR << "TcpConnection::handleRead";
        handleError();
    }
}

void TcpConnection::handleWrite() {
    if(channel_->isWriting()) {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if(n > 0) {
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting();
                if(writeCompleteCallback_) {
                    loop_->queueInLoop(
                            std::bind(writeCompleteCallback_, shared_from_this())
                            );
                }
                if(state_ == kDisconnecting) {
                    shutdownInLoop();
                }
            }
        }else if(n < 0) {
            errno = savedErrno;
            LOG_ERROR << "TcpConnection::handleWrite";
        }
    }else {
        LOG_ERROR << "TcpConnection fd=" << channel_->fd() << " is down, no more writing";
    }
}

void TcpConnection::handleClose() {
    LOG_INFO << "TcpConnection::handleClose fd=" << channel_->fd()
             << " state=" << static_cast<int>(state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    if (connectionCallback_) {
        connectionCallback_(connPtr);
    }
    if (closeCallback_) {
        closeCallback_(connPtr);
    }
}

void TcpConnection::handleError() {
    int optVal;
    socklen_t optLen = sizeof(optVal);
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optVal, &optLen) < 0) {
        err = errno;
    }else {
        err = optVal;
    }

    LOG_ERROR << "TcpConnection::handleError name:" << name_ << " - SO_ERROR:" << err;
    handleClose();
}

}  // namespace jpmuduo
