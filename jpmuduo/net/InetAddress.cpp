//
// Created by jianp on 2025/11/8.
//

#include "jpmuduo/net/InetAddress.h"

#include <strings.h>
#include <string.h>
#include <unistd.h>

namespace jpmuduo {

InetAddress::InetAddress(uint16_t port, std::string ip) {
    bzero(&addr_, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}

std::string InetAddress::toIp() const {
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));

    return buf;
}
std::string InetAddress::toIpPort() const {
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    size_t  end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf + end, ":%u", port);

    return buf;
}
uint16_t InetAddress::toPort() const {
    return ntohs(addr_.sin_port);
}

InetAddress InetAddress::getLocalAddress(int sockfd) {
    sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    socklen_t addrlen = sizeof(addr);
    if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&addr), &addrlen) < 0) {
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        addr.sin_addr.s_addr = INADDR_ANY;
    }
    return InetAddress(addr);
}

}  // namespace jpmuduo