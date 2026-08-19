//
// Created by jianp on 2025/12/10.
//

#include "jpmuduo/net/Socket.h"
#include "jpmuduo/base/Logging.h"
#include "jpmuduo/base/Types.h"  // memZero
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/net/SocketsOps.h"

#include <stdio.h>  // snprintf
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <netinet/in.h>

namespace jpmuduo {

Socket::~Socket() {
    sockets::close(sockfd_);
}

bool Socket::getTcpInfo(struct tcp_info* tcpi) const
{
    socklen_t len = sizeof(*tcpi);
    memZero(tcpi, len);
    return ::getsockopt(sockfd_, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

bool Socket::getTcpInfoString(char* buf, int len) const
{
    struct tcp_info tcpi;
    bool ok = getTcpInfo(&tcpi);
    if (ok)
    {
        snprintf(buf, len, "unrecovered=%u "
                 "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
                 "lost=%u retrans=%u rtt=%u rttvar=%u "
                 "sshthresh=%u cwnd=%u total_retrans=%u",
                 tcpi.tcpi_retransmits,  // Number of unrecovered [RTO] timeouts
                 tcpi.tcpi_rto,          // Retransmit timeout in usec
                 tcpi.tcpi_ato,          // Predicted tick of soft clock in usec
                 tcpi.tcpi_snd_mss,
                 tcpi.tcpi_rcv_mss,
                 tcpi.tcpi_lost,         // Lost packets
                 tcpi.tcpi_retrans,      // Retransmitted packets out
                 tcpi.tcpi_rtt,          // Smoothed round trip time in usec
                 tcpi.tcpi_rttvar,       // Medium deviation
                 tcpi.tcpi_snd_ssthresh,
                 tcpi.tcpi_snd_cwnd,
                 tcpi.tcpi_total_retrans);  // Total retransmits for entire connection
    }
    return ok;
}

int Socket::createTcpSocket() {
    return sockets::createNonblockingOrDie(AF_INET);
}

int Socket::createUdpSocket() {
    int sockfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0) {
        LOG_SYSFATAL << "create UDP socket error";
    }
    return sockfd;
}

void Socket::bindAddress(const InetAddress &localaddr) {
    sockets::bindOrDie(sockfd_, sockets::sockaddr_cast(localaddr.getSockAddr()));
}

void Socket::listen() {
    sockets::listenOrDie(sockfd_);
}

int Socket::accept(InetAddress *peeraddr) {
    sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    int connfd = sockets::accept(sockfd_, &addr);
    if(connfd >= 0) {
        peeraddr->setSockAddr(addr);
    }

    return connfd;
}

void Socket::shutdownWrite() {
    sockets::shutdownWrite(sockfd_);
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