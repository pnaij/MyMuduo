//
// Created by jianp on 2025/11/8.
//

#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/net/Endian.h"
#include "jpmuduo/net/SocketsOps.h"

#include <strings.h>
#include <string.h>
#include <unistd.h>

namespace jpmuduo {

InetAddress::InetAddress(uint16_t port, std::string ip) {
    sockets::fromIpPort(ip.c_str(), port, &addr_);
}

std::string InetAddress::toIp() const {
    char buf[64] = {0};
    sockets::toIp(buf, sizeof(buf), sockets::sockaddr_cast(&addr_));

    return buf;
}
std::string InetAddress::toIpPort() const {
    char buf[64] = {0};
    sockets::toIpPort(buf, sizeof(buf), sockets::sockaddr_cast(&addr_));

    return buf;
}
uint16_t InetAddress::toPort() const {
    return sockets::networkToHost16(addr_.sin_port);
}

InetAddress InetAddress::getLocalAddress(int sockfd) {
    return InetAddress(sockets::getLocalAddr(sockfd));
}

}  // namespace jpmuduo