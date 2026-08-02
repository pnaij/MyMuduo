#include "jpmuduo/net/TcpClient.h"
#include "jpmuduo/net/TcpServer.h"
#include "jpmuduo/net/EventLoopThread.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/net/Buffer.h"

#include <stdio.h>
#include <string>
#include <unistd.h>

using namespace jpmuduo;

// Test: TcpClient connecting to an echo server
// Phase 1: Connect, send "Hello", receive echo, verify
// Phase 2: Disconnect (client-initiated via conn->shutdown)
// Phase 3: Reconnect (with enableRetry), send "World", receive echo, verify

int main() {
    // Start echo server in a background thread
    EventLoopThread serverThread;
    EventLoop* serverLoop = serverThread.startLoop();
    InetAddress listenAddr(12345);
    TcpServer server(serverLoop, listenAddr, "EchoServer");

    // Server echoes back any received message
    server.setMessageCallback([](const TcpConnectionPtr& conn, Buffer* buf, TimeStamp) {
        std::string msg = buf->retrieveAllAsString();
        printf("[Server] echoing: '%s'\n", msg.c_str());
        conn->send(msg);
    });
    server.start();

    // Give server time to start listening
    usleep(200000);

    // Client event loop in main thread
    EventLoop clientLoop;
    InetAddress serverAddr(12345);
    TcpClient client(&clientLoop, serverAddr, "TestClient");

    // Test state machine
    enum State {
        INIT,          // waiting for initial connection
        CONNECTED,     // connected, will send "Hello"
        ECHO_RECEIVED, // echo verified, will enable retry + disconnect
        DISCONNECTED,  // disconnect detected, retry is in progress
        RECONNECTED,   // reconnected, will send "World"
        ECHO2_RECEIVED,// second echo verified
        DONE           // test complete
    };
    State state = INIT;
    bool pass = true;

    client.setConnectionCallback([&](const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            if (state == INIT) {
                printf("[Test] Phase 1: Connected, sending 'Hello'\n");
                state = CONNECTED;
                conn->send("Hello");
            } else if (state == DISCONNECTED) {
                printf("[Test] Phase 3: Reconnected, sending 'World'\n");
                state = RECONNECTED;
                conn->send("World");
            }
        } else {
            // Disconnected
            if (state == ECHO_RECEIVED) {
                printf("[Test] Phase 2: Disconnected\n");
                state = DISCONNECTED;
            }
        }
    });

    client.setMessageCallback([&](const TcpConnectionPtr& conn, Buffer* buf, TimeStamp) {
        std::string msg = buf->retrieveAllAsString();
        printf("[Test] Received: '%s' (state=%d)\n", msg.c_str(), (int)state);

        if (state == CONNECTED && msg == "Hello") {
            printf("[Test] Phase 1: Echo verified. Enabling retry and disconnecting...\n");
            state = ECHO_RECEIVED;
            client.enableRetry();
            conn->shutdown(); // half-close: triggers server close -> client disconnect -> retry
        } else if (state == RECONNECTED && msg == "World") {
            printf("[Test] Phase 3: Echo2 verified. Test complete!\n");
            state = DONE;
            clientLoop.quit();
        }
    });

    client.connect();

    // Timeout to prevent infinite loop in case of test failure
    clientLoop.runAfter(5.0, [&]() {
        if (state != DONE) {
            printf("[Test] TIMEOUT reached in state=%d\n", (int)state);
        }
        clientLoop.quit();
    });

    clientLoop.loop();

    pass = (state == DONE);
    printf("\n=== RESULT: %s ===\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
