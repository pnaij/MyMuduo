// InetAddress_unittest.cpp
//
// 仿照 muduo/muduo/net/tests/InetAddress_unittest.cc 重写（自包含 main + CHECK），
// 针对 jpmuduo::InetAddress 验证：
//   - 构造 (port, ip) 字符串 ↔ 地址往返转换（toIp / toIpPort / toPort）
//   - 端口边界值、IP 各段边界值
//   - getLocalAddress 从已绑定 socket 取回本机地址
//   - resolve 到已知 IP：jpmuduo 无 InetAddress::resolve，测试内用
//     getaddrinfo 做解析并把 sockaddr_in 交给 InetAddress 往返验证
//
// 注意 jpmuduo 构造签名与原始 muduo 不同：
//   muduo:      InetAddress(port, loopbackOnly=false, ipv6=false) / InetAddress(ip, port)
//   jpmuduo:    InetAddress(port, ip="127.0.0.1")   —— 参数顺序为 (端口, IP)
//   且没有 port() 方法，用 toPort()；也没有 ipv6 支持。

#include "jpmuduo/net/InetAddress.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

using jpmuduo::InetAddress;
using std::string;

static int g_failures = 0;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #expr);                                        \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static void testInetAddress() {
    // 默认 IP 是 127.0.0.1（与原始 muduo 的默认 0.0.0.0 不同）
    InetAddress addr0(1234);
    printf("addr0: %s\n", addr0.toIpPort().c_str());
    CHECK(addr0.toIp() == string("127.0.0.1"));
    CHECK(addr0.toIpPort() == string("127.0.0.1:1234"));
    CHECK(addr0.toPort() == 1234);

    InetAddress addr1(4321, "127.0.0.1");
    CHECK(addr1.toIp() == string("127.0.0.1"));
    CHECK(addr1.toIpPort() == string("127.0.0.1:4321"));
    CHECK(addr1.toPort() == 4321);

    InetAddress addr2(8888, "1.2.3.4");
    CHECK(addr2.toIp() == string("1.2.3.4"));
    CHECK(addr2.toIpPort() == string("1.2.3.4:8888"));
    CHECK(addr2.toPort() == 8888);

    // 端口与 IP 各段边界值
    InetAddress addr3(65535, "255.254.253.252");
    CHECK(addr3.toIp() == string("255.254.253.252"));
    CHECK(addr3.toIpPort() == string("255.254.253.252:65535"));
    CHECK(addr3.toPort() == 65535);

    InetAddress addr4(0, "0.0.0.0");
    CHECK(addr4.toIp() == string("0.0.0.0"));
    CHECK(addr4.toIpPort() == string("0.0.0.0:0"));
    CHECK(addr4.toPort() == 0);

    InetAddress addr5(80, "192.168.1.1");
    CHECK(addr5.toIp() == string("192.168.1.1"));
    CHECK(addr5.toIpPort() == string("192.168.1.1:80"));
    CHECK(addr5.toPort() == 80);
}

static void testGetLocalAddress() {
    // 绑定一个本地 socket（动态端口），getLocalAddress 应取回绑定地址
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    CHECK(fd >= 0);
    if (fd < 0)
        return;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // 127.0.0.1
    addr.sin_port = 0;                              // 动态端口
    CHECK(::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0);

    InetAddress local = InetAddress::getLocalAddress(fd);
    printf("getLocalAddress: %s\n", local.toIpPort().c_str());
    CHECK(local.toIp() == string("127.0.0.1"));
    CHECK(local.toPort() != 0);

    ::close(fd);
}

// jpmuduo 没有 InetAddress::resolve()，这里用 getaddrinfo 解析已知 IP 后
// 构造 InetAddress 做往返验证（不监听任何端口，无端口冲突）。
static void testResolveToKnownIp() {
    // 1) 数字 IP：纯本地转换，不依赖 DNS
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST;

    struct addrinfo* result = NULL;
    int rc = ::getaddrinfo("127.0.0.1", NULL, &hints, &result);
    CHECK(rc == 0 && result != NULL);
    if (rc == 0 && result != NULL) {
        InetAddress addr(*(reinterpret_cast<const struct sockaddr_in*>(
            result->ai_addr)));
        printf("resolved 127.0.0.1 -> %s\n", addr.toIpPort().c_str());
        CHECK(addr.toIp() == string("127.0.0.1"));
        ::freeaddrinfo(result);
    }

    // 2) 主机名 localhost（来自 /etc/hosts，不依赖外网 DNS）
    hints.ai_flags = 0;
    rc = ::getaddrinfo("localhost", NULL, &hints, &result);
    CHECK(rc == 0 && result != NULL);
    if (rc == 0 && result != NULL) {
        InetAddress addr(*(reinterpret_cast<const struct sockaddr_in*>(
            result->ai_addr)));
        printf("resolved localhost -> %s\n", addr.toIpPort().c_str());
        CHECK(addr.toIp() == string("127.0.0.1"));
        CHECK(addr.toPort() == 0);
        ::freeaddrinfo(result);
    } else {
        printf("warning: getaddrinfo(\"localhost\") failed rc=%d\n", rc);
    }
}

static void testSockAddrRoundTrip() {
    // 直接构造 sockaddr_in 后交给 InetAddress（第二个构造函数）
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(9999);
    CHECK(::inet_pton(AF_INET, "10.0.0.1", &sa.sin_addr) == 1);

    InetAddress addr(sa);
    CHECK(addr.toIp() == string("10.0.0.1"));
    CHECK(addr.toPort() == 9999);
    CHECK(addr.toIpPort() == string("10.0.0.1:9999"));

    // getSockAddr 返回的地址信息应与输入一致
    const struct sockaddr_in* back = addr.getSockAddr();
    CHECK(back->sin_port == htons(9999));
    CHECK(back->sin_addr.s_addr == sa.sin_addr.s_addr);
}

int main() {
    testInetAddress();
    testGetLocalAddress();
    testResolveToKnownIp();
    testSockAddrRoundTrip();

    if (g_failures > 0) {
        fprintf(stderr, "InetAddress_unittest: %d check(s) failed\n", g_failures);
        return 1;
    }
    printf("InetAddress_unittest: all checks passed\n");
    return 0;
}
