//
// http_test.cpp - HTTP server demo
//

#include "jpmuduo/net/http/HttpServer.h"
#include "jpmuduo/net/http/HttpRequest.h"
#include "jpmuduo/net/http/HttpResponse.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/base/Logger.h"

#include <string>

using namespace jpmuduo;

int main() {
    EventLoop loop;
    InetAddress addr(8080);

    HttpServer server(&loop, addr, "HttpServer-01");
    server.setThreadNum(4);

    server.setHttpCallback([](const HttpRequest& req, HttpResponse* resp) {
        std::string path = req.path();

        if (path == "/") {
            resp->setContentType("text/html");
            resp->setBody("<h1>muduoSelf HTTP Server</h1><p>GET /api/hello for JSON</p>");
        } else if (path == "/api/hello") {
            resp->setContentType("application/json");
            resp->setBody("{\"message\":\"Hello from muduoSelf!\"}");
        } else {
            resp->setStatusCode(k404NotFound);
            resp->setContentType("text/plain");
            resp->setBody("404 - Not Found: " + path);
        }
    });

    server.start();
    LOG_INFO("HTTP server listening on port 8080\n");
    loop.loop();

    return 0;
}
