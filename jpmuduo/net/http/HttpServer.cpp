//
// HttpServer.cpp - HTTP server implementation
//

#include "jpmuduo/net/http/HttpServer.h"
#include "jpmuduo/net/http/HttpContext.h"
#include "jpmuduo/net/Buffer.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/base/Logger.h"

#include <functional>
#include <algorithm>

namespace jpmuduo {

HttpServer::HttpServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       const std::string& name)
    : server_(loop, listenAddr, name) {
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void HttpServer::start() {
    server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr& conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn->connected()) {
        auto data = std::unique_ptr<HttpConnectionData>(new HttpConnectionData());
        connections_[conn->name()] = std::move(data);
    } else {
        auto it = connections_.find(conn->name());
        if (it != connections_.end()) {
            if (it->second->timerScheduled) {
                conn->getLoop()->cancel(it->second->idleTimer);
            }
            connections_.erase(it);
        }
    }
}

void HttpServer::onMessage(const TcpConnectionPtr& conn,
                            Buffer* buf, TimeStamp) {
    HttpConnectionData* data = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(conn->name());
        if (it == connections_.end()) {
            conn->shutdown();
            return;
        }
        data = it->second.get();
        if (data->timerScheduled) {
            conn->getLoop()->cancel(data->idleTimer);
            data->timerScheduled = false;
        }
    }

    HttpContext* ctx = &data->context;
    bool keepAlive = data->keepAlive;

    while (ctx->parseRequest(buf)) {
        requestCount_.fetch_add(1);
        onRequest(conn, ctx->request());
        keepAlive = data->keepAlive;
        ctx->reset();
    }

    if (ctx->hasError()) {
        conn->shutdown();
        return;
    }
    if (buf->readableBytes() > 64 * 1024) {
        conn->shutdown();
        return;
    }

    if (keepAlive && conn->connected()) {
        startIdleTimer(conn, conn->name());
    }
}

void HttpServer::onRequest(const TcpConnectionPtr& conn,
                            const HttpRequest& req) {
    const std::string& connection = req.getHeader("connection");
    bool close = false;

    if (connection == "close") {
        close = true;
    }

    HttpResponse resp(close);
    resp.setStatusCode(k200Ok);

    if (httpCallback_) {
        httpCallback_(req, &resp);
    }

    // Default error body for 4xx/5xx
    if (resp.body().empty() && resp.statusCode() >= 400) {
        const char* phrase = HttpResponse::reasonPhrase(
            static_cast<HttpStatusCode>(resp.statusCode()));
        std::string html = "<html><head><title>";
        html += phrase;
        html += "</title></head><body><h1>";
        html += std::to_string(resp.statusCode());
        html += " ";
        html += phrase;
        html += "</h1><hr><em>muduoSelf/1.0</em></body></html>";
        resp.setBody(html);
    }

    // Auto-detect Content-Type from path (only for default error pages)
    if (!resp.body().empty() && resp.statusCode() >= 400) {
        resp.addHeader("Content-Type", guessContentType(req.path()));
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(conn->name());
        if (it != connections_.end()) {
            it->second->keepAlive = !close;
        }
    }

    Buffer buf;
    resp.appendToBuffer(&buf);
    conn->send(std::string(buf.peek(), buf.readableBytes()));

    if (resp.closeConnection()) {
        conn->shutdown();
    }
}

void HttpServer::startIdleTimer(const TcpConnectionPtr& conn,
                                 const std::string& connName) {
    TcpConnectionPtr connPtr = conn;
    TimerId timer = conn->getLoop()->runAfter(
        static_cast<double>(idleTimeout_),
        [this, connPtr, connName]() {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = connections_.find(connName);
                if (it == connections_.end() || !it->second->timerScheduled) {
                    return;
                }
                it->second->timerScheduled = false;
            }
            connPtr->shutdown();
        });

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(connName);
    if (it != connections_.end()) {
        it->second->idleTimer = timer;
        it->second->timerScheduled = true;
    }
}

std::string HttpServer::guessContentType(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return "text/plain";
    std::string ext = path.substr(dot);
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css")   return "text/css; charset=utf-8";
    if (ext == ".js")    return "application/javascript; charset=utf-8";
    if (ext == ".json")  return "application/json; charset=utf-8";
    if (ext == ".xml")   return "application/xml; charset=utf-8";
    if (ext == ".png")   return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")   return "image/gif";
    if (ext == ".svg")   return "image/svg+xml";
    if (ext == ".ico")   return "image/x-icon";
    if (ext == ".txt")   return "text/plain; charset=utf-8";
    if (ext == ".pdf")   return "application/pdf";
    return "application/octet-stream";
}

}  // namespace jpmuduo
