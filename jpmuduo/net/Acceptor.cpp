//
// Created by jianp on 2025/12/10.
//

#include "jpmuduo/net/Acceptor.h"
#include "jpmuduo/base/Logging.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/net/Socket.h"
#include "jpmuduo/net/SocketsOps.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

namespace jpmuduo {

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(Socket::createTcpSocket())
    , acceptChannel_(loop, acceptSocket_.fd())
    , idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
    , listening_(false) {
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    sockets::close(idleFd_);
}

void Acceptor::listen() {
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead() {
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0) {
        if(newConnectionCallback_) {
            newConnectionCallback_(connfd, peerAddr);
        }else {
            sockets::close(connfd);
        }
    }else {
        LOG_SYSERR << "accept error";
        if(errno == EMFILE) {
            LOG_ERROR << "sockfd reached limit!";
            sockets::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_.fd(), nullptr, nullptr);
            sockets::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }

}

}  // namespace jpmuduo
