//
// Quick test for UdpServer / UdpClient
//

#include "jpmuduo/net/UdpServer.h"
#include "jpmuduo/net/UdpClient.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/base/Logging.h"
#include "jpmuduo/net/Buffer.h"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace jpmuduo;

int main() {
    // Test 1: UdpServer echo
    std::cout << "=== Test 1: UdpServer echo ===" << std::endl;

    EventLoop loop;
    InetAddress listenAddr(9876);

    UdpServer server(&loop, listenAddr, "udp-echo");
    server.setMessageCallback([&server](Buffer* buf, TimeStamp ts, const InetAddress& sender) {
        std::string msg = buf->retrieveAllAsString();
        std::cout << "Server received: \"" << msg << "\" from " << sender.toIpPort() << std::endl;
        server.sendTo(msg, sender);
        std::cout << "Server echoed back." << std::endl;
    });
    server.start();

    // Run the server in a separate thread
    std::thread serverThread([&loop]() {
        loop.loop();
    });

    // Give the server a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Test 2: UdpClient sendTo + recvfrom
    std::cout << "\n=== Test 2: UdpClient send/recv ===" << std::endl;

    EventLoop clientLoop;
    InetAddress serverAddr(9876);
    UdpClient client(&clientLoop, serverAddr, "udp-client");

    bool gotResponse = false;
    client.setMessageCallback([&](Buffer* buf, TimeStamp ts, const InetAddress& sender) {
        std::string msg = buf->retrieveAllAsString();
        std::cout << "Client received: \"" << msg << "\" from " << sender.toIpPort() << std::endl;
        gotResponse = true;
        clientLoop.quit();
    });

    // Run client loop in another thread
    std::thread clientThread([&clientLoop]() {
        clientLoop.loop();
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
        clientLoop.quit();
    }

    loop.quit();
    clientThread.join();
    serverThread.join();

    return gotResponse ? 0 : 1;
}
