//
// Created by jianp on 2025/12/10.
//

#ifndef JPMUDUO_SOCKET_H
#define JPMUDUO_SOCKET_H

#include <netinet/tcp.h>  // struct tcp_info
#include <sys/types.h>

#include "jpmuduo/base/noncopyable.h"

namespace jpmuduo {

class InetAddress;

class Socket : noncopyable {
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    ~Socket();

    int fd() const { return sockfd_; }

    static int createTcpSocket();
    static int createUdpSocket();

    // return true if success.
    bool getTcpInfo(struct tcp_info*) const;
    bool getTcpInfoString(char* buf, int len) const;

    void bindAddress(const InetAddress& localaddr);
    void listen();
    int accept(InetAddress* peeraddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);
    void setBroadcast(bool on);

    ssize_t recvfrom(void* buf, size_t len, InetAddress* peerAddr);
    ssize_t sendto(const void* buf, size_t len, const InetAddress& peerAddr);

private:
    const int sockfd_;
};


}  // namespace jpmuduo

#endif //JPMUDUO_SOCKET_H
