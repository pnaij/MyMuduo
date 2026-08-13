#include "jpmuduo/net/inspector/Inspector.h"
#include "jpmuduo/net/http/HttpServer.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/InetAddress.h"
#include "jpmuduo/net/TcpServer.h"
#include "jpmuduo/base/Logging.h"

using namespace jpmuduo;

int main() {
    EventLoop loop;

    // HTTP app server
    HttpServer http(&loop, InetAddress(8080), "app");
    http.setHttpCallback([](const HttpRequest& req, HttpResponse* resp) {
        if (req.path() == "/") {
            resp->setContentType("text/html");
            resp->setBody("<h1>Hello</h1>");
        } else {
            resp->setStatusCode(k404NotFound);
        }
    });
    http.start();

    // Inspector on port 9090
    Inspector inspector(&loop, 9090);
    inspector.setConnectionStats([&http]() {
        (void)http;
        return "(callback works)";
    });
    inspector.start();

    loop.loop();
    return 0;
}
