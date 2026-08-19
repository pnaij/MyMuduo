// 对照原始 muduo SocketsOps.h 改写：
//   - namespace muduo::net::sockets -> jpmuduo::sockets
//   - jpmuduo 仅支持 IPv4，去掉 sockaddr_in6 相关接口
//   - 原始头文件注释"internal header, you should not include this"
//     表示仅供 net 内部使用，此处同样不对外承诺稳定接口

#ifndef JPMUDUO_NET_SOCKETSOPS_H
#define JPMUDUO_NET_SOCKETSOPS_H

#include <arpa/inet.h>

namespace jpmuduo {
namespace sockets {

///
/// Creates a non-blocking socket file descriptor,
/// abort if any error.
int createNonblockingOrDie(sa_family_t family);

int connect(int sockfd, const struct sockaddr* addr);
void bindOrDie(int sockfd, const struct sockaddr* addr);
void listenOrDie(int sockfd);
int accept(int sockfd, struct sockaddr_in* addr);
ssize_t read(int sockfd, void* buf, size_t count);
ssize_t readv(int sockfd, const struct iovec* iov, int iovcnt);
ssize_t write(int sockfd, const void* buf, size_t count);
void close(int sockfd);
void shutdownWrite(int sockfd);

void toIpPort(char* buf, size_t size, const struct sockaddr* addr);
void toIp(char* buf, size_t size, const struct sockaddr* addr);
void fromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr);

int getSocketError(int sockfd);

const struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr);
struct sockaddr* sockaddr_cast(struct sockaddr_in* addr);
const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr* addr);

struct sockaddr_in getLocalAddr(int sockfd);
struct sockaddr_in getPeerAddr(int sockfd);
bool isSelfConnect(int sockfd);

}  // namespace sockets
}  // namespace jpmuduo

#endif  // JPMUDUO_NET_SOCKETSOPS_H
