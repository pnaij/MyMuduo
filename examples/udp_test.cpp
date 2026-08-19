//
// Quick test for UdpServer / UdpClient
//

#include "jpmuduo/net/UdpServer.h"
#include "jpmuduo/net/UdpClient.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/EventLoopThread.h"
#include "jpmuduo/base/Logging.h"
#include "jpmuduo/net/Buffer.h"

#include <iostream>
#include <string>
#include <chrono>

using namespace jpmuduo;

int main() {
    // Test 1: UdpServer echo
    std::cout << "=== Test 1: UdpServer echo ===" << std::endl;

    // EventLoop 线程绑定：EventLoopThread 在子线程创建并运行 loop，
    // 主线程通过 runInLoop 跨线程投递（符合 muduo 线程模型）
    EventLoopThread serverLoopThread;
    EventLoop* loop = serverLoopThread.startLoop();
    InetAddress listenAddr(9876);

    UdpServer server(loop, listenAddr, "udp-echo");
    server.setMessageCallback([&server](Buffer* buf, TimeStamp ts, const InetAddress& sender) {
        std::string msg = buf->retrieveAllAsString();
        std::cout << "Server received: \"" << msg << "\" from " << sender.toIpPort() << std::endl;
        server.sendTo(msg, sender);
        std::cout << "Server echoed back." << std::endl;
    });
    server.start();

    // Give the server a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Test 2: UdpClient sendTo + recvfrom
    std::cout << "\n=== Test 2: UdpClient send/recv ===" << std::endl;

    EventLoopThread clientLoopThread;
    EventLoop* clientLoop = clientLoopThread.startLoop();
    InetAddress serverAddr(9876);
    UdpClient client(clientLoop, serverAddr, "udp-client");

    bool gotResponse = false;
    client.setMessageCallback([&](Buffer* buf, TimeStamp ts, const InetAddress& sender) {
        std::string msg = buf->retrieveAllAsString();
        std::cout << "Client received: \"" << msg << "\" from " << sender.toIpPort() << std::endl;
        gotResponse = true;
        clientLoop->quit();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    client.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    client.sendTo("hello udp!", 10, serverAddr);
    std::cout << "Client sent: hello udp! to " << serverAddr.toIpPort() << std::endl;

    // Wait for response
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (gotResponse) {
        std::cout << "\n*** ALL TESTS PASSED ***" << std::endl;
    } else {
        std::cout << "\n*** TEST FAILED: No response received ***" << std::endl;
        clientLoop->quit();
    }

    // EventLoopThread 析构时 quit + join
    loop->quit();
    return gotResponse ? 0 : 1;
}
