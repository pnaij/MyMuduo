//
// Created by jianp on 2025/12/11.
//

#include "jpmuduo/net/TcpConnection.h"
#include "jpmuduo/base/Logging.h"
#include "jpmuduo/net/Socket.h"
#include "jpmuduo/net/SocketsOps.h"
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
    // 析构时连接必须已断开（否则说明上层未正确回收连接）
    assert(state_ == kDisconnected);
}

bool TcpConnection::getTcpInfo(struct tcp_info* tcpi) const {
    return socket_->getTcpInfo(tcpi);
}

std::string TcpConnection::getTcpInfoString() const {
    char buf[1024];
    buf[0] = '\0';
    socket_->getTcpInfoString(buf, sizeof buf);
    return buf;
}

void TcpConnection::send(const std::string &buf) {
    if(state_ == kConnected) {
        if(loop_->isInLoopThread()) {
            sendInLoop(buf.c_str(), buf.size());
        }else {
            // copy buf to avoid dangling pointer on cross-thread call；
            // shared_from_this 保证对象活到任务执行完（裸 this 有延迟窗口悬空风险）
            loop_->runInLoop([self = shared_from_this(), copy = std::string(buf)]() {
                self->sendInLoop(copy.data(), copy.size());
            });
        }
    }
}

void TcpConnection::send(Buffer* buf) {
    if(state_ == kConnected) {
        if(loop_->isInLoopThread()) {
            sendInLoop(buf);
            buf->retrieveAll();  // 数据已被 TcpConnection 取走（swap 语义）
        }else {
            // 跨线程必须拷贝：Buffer 属于调用者，指针传给另一线程是悬空风险
            loop_->runInLoop(
                    [self = shared_from_this(), msg = buf->retrieveAllAsString()]() {
                        self->sendInLoop(msg.data(), msg.size());
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
            loop_->runInLoop([self = shared_from_this(),
                              copy = std::string(static_cast<const char*>(data), len)]() {
                self->sendInLoop(copy.data(), copy.size());
            });
        }
    }
}

void TcpConnection::sendInLoop(const void *message, size_t len) {
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    // 仅 kDisconnected 拒绝写入：kDisconnecting 是"数据还没发完"的半关闭
    // 状态，此时必须继续写（写完 handleWrite 才 shutdown），
    // 原实现检查 kDisconnecting 会错误丢弃最后的业务数据
    if(state_ == kDisconnected) {
        LOG_ERROR << "disconnected, give up writing!";
        return ;
    }

    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = sockets::write(channel_->fd(), message, len);
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
        // 用 shared_from_this() 而非裸 this：闭包入队到执行之间存在延迟窗口，
        // 若期间对象被析构（如调用方退出），裸 this 会悬空（原始 muduo 的
        // FIXME: shared_from_this()? 即指此问题）；强引用保证对象活到任务执行完
        loop_->runInLoop(
                std::bind(&TcpConnection::shutdownInLoop, shared_from_this())
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

void TcpConnection::forceClose() {
    // 强关：模拟"读到 0 字节"，立即走 handleClose 全关
    // （对比 shutdown：优雅半关，等数据发完再发 FIN）
    if(state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);
        // queueInLoop 而非 runInLoop：forceCloseInLoop 会触发用户回调
        // 与 closeCallback（可能析构对象），必须延迟到当前事件处理完成，
        // 避免在调用栈中同步重入；shared_from_this 保证对象活到任务执行完
        loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
    }
}

void TcpConnection::forceCloseWithDelay(double seconds) {
    if(state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);
        // 定时器回调用弱引用：到期时对象若已析构则不执行
        // （定时器与队列不同——回调可能永远不跑，弱引用防悬空）
        loop_->runAfter(seconds,
                        makeWeakCallback(shared_from_this(), &TcpConnection::forceClose));
    }
}

void TcpConnection::forceCloseInLoop() {
    loop_->assertInLoopThread();
    if(state_ == kConnected || state_ == kDisconnecting) {
        handleClose();
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
    // 防止重复调用（原始 muduo 断言一致）
    assert(state_ == kConnecting);
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

const char* TcpConnection::stateToString() const {
    switch (state_) {
    case kDisconnected:
        return "kDisconnected";
    case kConnecting:
        return "kConnecting";
    case kConnected:
        return "kConnected";
    case kDisconnecting:
        return "kDisconnecting";
    default:
        return "unknown state";
    }
}

void TcpConnection::handleClose() {
    LOG_INFO << "TcpConnection::handleClose fd=" << channel_->fd()
             << " state=" << stateToString();
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
    int err = sockets::getSocketError(channel_->fd());

    LOG_ERROR << "TcpConnection::handleError name:" << name_ << " - SO_ERROR:" << err;
    // 仅记录错误，不主动 handleClose（原始 muduo 语义）：
    // 多数错误（如对端 RST）最终会以 read 0 字节/POLLERR 走 handleClose，
    // 主动关闭可能过早断开仍可挽救的连接
}

}  // namespace jpmuduo
