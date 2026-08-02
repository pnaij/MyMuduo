//
// HttpServer.h - HTTP server on top of TcpServer
//

#ifndef JPMUDUO_HTTPSERVER_H
#define JPMUDUO_HTTPSERVER_H

#include "jpmuduo/net/TcpServer.h"
#include "jpmuduo/net/http/HttpContext.h"
#include "jpmuduo/net/http/HttpRequest.h"
#include "jpmuduo/net/http/HttpResponse.h"
#include "jpmuduo/base/noncopyable.h"

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace jpmuduo {

class EventLoop;

class HttpServer : noncopyable {
public:
    using HttpCallback = std::function<void(const HttpRequest&, HttpResponse*)>;

    HttpServer(EventLoop* loop,
               const InetAddress& listenAddr,
               const std::string& name);

    void setHttpCallback(const HttpCallback& cb) { httpCallback_ = cb; }
    void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }
    void setIdleTimeout(int seconds) { idleTimeout_ = seconds; }
    void start();

    int64_t requestCount() const { return requestCount_.load(); }

private:
    struct HttpConnectionData {
        HttpContext context;
        TimerId idleTimer;
        bool timerScheduled = false;
        bool keepAlive = true;
    };

    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, TimeStamp);
    void onRequest(const TcpConnectionPtr& conn, const HttpRequest& req);
    void startIdleTimer(const TcpConnectionPtr& conn, const std::string& connName);
    static std::string guessContentType(const std::string& path);

    TcpServer server_;
    HttpCallback httpCallback_;
    std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<HttpConnectionData>> connections_;
    std::atomic<int64_t> requestCount_{0};
    int idleTimeout_{30};
};

}  // namespace jpmuduo

#endif
