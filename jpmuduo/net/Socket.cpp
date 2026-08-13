//
// Created by jianp on 2025/12/10.
//

#include "jpmuduo/net/Socket.h"
#include "jpmuduo/base/Logging.h"
#include "jpmuduo/net/InetAddress.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <netinet/in.h>

namespace jpmuduo {

Socket::~Socket() {
    close(sockfd_);
}

int Socket::createTcpSocket() {
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0) {
        LOG_SYSFATAL << "create TCP socket error";
    }
    return sockfd;
}

int Socket::createUdpSocket() {
    int sockfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0) {
        LOG_SYSFATAL << "create UDP socket error";
    }
    return sockfd;
}

void Socket::bindAddress(const InetAddress &localaddr) {
    if(::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)) != 0) {
        LOG_SYSFATAL << "bind socket:" << sockfd_ << " fail";
    }
}

void Socket::listen() {
    if(::listen(sockfd_, 65535) != 0) {
        LOG_SYSFATAL << "listen sockfd:" << sockfd_ << " fail";
    }
}

int Socket::accept(InetAddress *peeraddr) {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    bzero(&addr, sizeof(addr));
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(connfd >= 0) {
        LOG_INFO << "accept success";
        peeraddr->setSockAddr(addr);
    }else {
        LOG_SYSERR << "accept failed on sockfd:" << sockfd_;
    }

    return connfd;
}

void Socket::shutdownWrite() {
    if(::shutdown(sockfd_, SHUT_WR) < 0) {
        LOG_SYSERR << "shutdownWrite error";
    }
}

void Socket::setTcpNoDelay(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

void Socket::setBroadcast(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
}

ssize_t Socket::recvfrom(void* buf, size_t len, InetAddress* peerAddr) {
    sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    bzero(&addr, sizeof(addr));
    ssize_t n = ::recvfrom(sockfd_, buf, len, 0, (sockaddr*)&addr, &addrLen);
    if (n >= 0 && peerAddr) {
        peerAddr->setSockAddr(addr);
    }
    return n;
}

ssize_t Socket::sendto(const void* buf, size_t len, const InetAddress& peerAddr) {
    return ::sendto(sockfd_, buf, len, 0,
                    (const sockaddr*)peerAddr.getSockAddr(),
                    sizeof(*peerAddr.getSockAddr()));
}

}  // namespace jpmuduo