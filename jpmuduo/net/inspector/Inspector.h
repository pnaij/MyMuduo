//
// Inspector.h - runtime HTTP inspector for process/system/performance info
//

#ifndef JPMUDUO_INSPECTOR_H
#define JPMUDUO_INSPECTOR_H

#include "jpmuduo/base/noncopyable.h"
#include "jpmuduo/net/http/HttpServer.h"
#include "jpmuduo/net/http/HttpRequest.h"
#include "jpmuduo/net/http/HttpResponse.h"

#include <functional>
#include <string>
#include <vector>

namespace jpmuduo {

class EventLoop;

class Inspector : noncopyable {
public:
    using StatsCallback = std::function<std::string()>;

    Inspector(EventLoop* loop, uint16_t port);

    void start();
    void stop();

    // Callbacks for feeding stats from external TcpServer/HttpServer instances
    void setConnectionStats(StatsCallback cb) { connectionStatsCb_ = std::move(cb); }
    void setRequestStats(StatsCallback cb) { requestStatsCb_ = std::move(cb); }

private:
    void onRequest(const HttpRequest& req, HttpResponse* resp);
    void handleOverview(const HttpRequest& req, HttpResponse* resp);
    void handleProcess(const HttpRequest& req, HttpResponse* resp);
    void handleSystem(const HttpRequest& req, HttpResponse* resp);
    void handlePerformance(const HttpRequest& req, HttpResponse* resp);

    // /proc helpers
    static std::string readProcFile(const char* path);
    static long getCpuCount();

    // HTML helpers
    static std::string htmlHeader(const std::string& title);
    static std::string htmlFooter();
    static std::string kvTable(const std::vector<std::pair<std::string, std::string>>& rows);

    HttpServer server_;
    StatsCallback connectionStatsCb_;
    StatsCallback requestStatsCb_;
    static const char kCss[];
};

}  // namespace jpmuduo

#endif
