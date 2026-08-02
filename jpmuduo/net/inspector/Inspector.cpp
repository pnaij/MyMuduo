//
// Inspector.cpp - runtime HTTP inspector
//

#include "jpmuduo/net/inspector/Inspector.h"
#include "jpmuduo/net/Buffer.h"

#include <sys/sysinfo.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>

namespace jpmuduo {

const char Inspector::kCss[] =
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
    "max-width:1200px;margin:0 auto;padding:20px;background:#1a1a2e;color:#e0e0e0}"
    "h1{color:#e94560;border-bottom:2px solid #e94560;padding-bottom:10px;font-size:24px}"
    "h2{color:#f0f0f0;margin:0 0 12px 0;font-size:16px;font-weight:500}"
    ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(350px,1fr));gap:16px}"
    ".card{background:#16213e;border-radius:8px;padding:16px 20px;"
    "box-shadow:0 2px 8px rgba(0,0,0,0.3);border:1px solid #0f3460}"
    "table{width:100%;border-collapse:collapse;font-size:13px}"
    "td{padding:4px 8px;border-bottom:1px solid #1a1a3e}"
    "td:first-child{color:#a0a0b0}"
    "td:last-child{text-align:right;font-family:'SF Mono',Monaco,monospace;color:#4ecca3}"
    ".bar{background:#1a1a3e;border-radius:3px;height:18px;overflow:hidden;margin-top:4px}"
    ".bar-fill{height:100%;border-radius:3px;transition:width 1s}"
    ".bar-green{background:#4ecca3}.bar-yellow{background:#f0a500}.bar-red{background:#e94560}"
    ".stat-row{display:flex;justify-content:space-between;padding:3px 0;font-size:13px}"
    ".stat-label{color:#a0a0b0}.stat-val{color:#4ecca3;font-family:monospace}"
    ".tag{display:inline-block;padding:2px 8px;border-radius:10px;font-size:11px;"
    "margin-right:4px}.tag-ok{background:#4ecca3;color:#111}.tag-warn{background:#f0a500;color:#111}"
    ".nav{display:flex;gap:8px;margin:16px 0}"
    ".nav a{text-decoration:none;color:#4ecca3;padding:4px 12px;border:1px solid #0f3460;"
    "border-radius:4px;font-size:12px}"
    ".nav a:hover{background:#0f3460}"
    ".refresh{font-size:11px;color:#666;float:right}"
;

Inspector::Inspector(EventLoop* loop, uint16_t port)
    : server_(loop, InetAddress(port), "Inspector") {
    server_.setHttpCallback(
        std::bind(&Inspector::onRequest, this,
                  std::placeholders::_1, std::placeholders::_2));
}

void Inspector::start() { server_.start(); }
void Inspector::stop()  {}

void Inspector::onRequest(const HttpRequest& req, HttpResponse* resp) {
    const std::string& path = req.path();
    if (path == "/" || path == "/inspector" || path == "/inspector/") {
        handleOverview(req, resp);
    } else if (path == "/inspector/process" || path == "/process") {
        handleProcess(req, resp);
    } else if (path == "/inspector/system" || path == "/system") {
        handleSystem(req, resp);
    } else if (path == "/inspector/performance" || path == "/performance") {
        handlePerformance(req, resp);
    } else {
        resp->setStatusCode(k404NotFound);
        resp->setBody("<html><body><h1>404 Not Found</h1></body></html>");
        resp->setContentType("text/html");
    }
}

// ─── Dashboard helpers ──────────────────────────────────────────

static std::string fmtMem(unsigned long bytes) {
    char b[32]; snprintf(b, sizeof(b), "%.1f MB", bytes / (1024.*1024.));
    return b;
}
static std::string bar(double pct) {
    const char* cls = pct > 80 ? "bar-red" : pct > 60 ? "bar-yellow" : "bar-green";
    char b[128];
    snprintf(b, sizeof(b), "<div class='bar'><div class='bar-fill %s' style='width:%.1f%%'></div></div>", cls, pct);
    return b;
}

void Inspector::handleOverview(const HttpRequest&, HttpResponse* resp) {
    std::string html = htmlHeader("muduoSelf Inspector");
    html += "<span class='refresh'>auto-refresh 5s</span>";
    html += "<div class='nav'><a href='/'>Dashboard</a><a href='/process'>Process</a><a href='/system'>System</a><a href='/performance'>Performance</a></div>";

    // ── System Summary ──
    struct sysinfo si;
    struct utsname uts;
    std::string hostname = "N/A", osname = "N/A";
    if (::uname(&uts) == 0) {
        hostname = uts.nodename;
        osname = std::string(uts.sysname) + " " + uts.release;
    }

    html += "<div class='grid'>";

    // Card 1: System Overview
    html += "<div class='card'><h2>System</h2>";
    html += "<div class='stat-row'><span class='stat-label'>Host</span><span class='stat-val'>" + hostname + "</span></div>";
    html += "<div class='stat-row'><span class='stat-label'>OS</span><span class='stat-val'>" + osname + "</span></div>";
    html += "<div class='stat-row'><span class='stat-label'>Arch</span><span class='stat-val'>" + std::string(uts.machine) + "</span></div>";
    html += "<div class='stat-row'><span class='stat-label'>CPU Cores</span><span class='stat-val'>" + std::to_string(getCpuCount()) + "</span></div>";

    if (::sysinfo(&si) == 0) {
        int d = si.uptime/86400, h = (si.uptime%86400)/3600, m = (si.uptime%3600)/60;
        char ub[32]; snprintf(ub, sizeof(ub), "%dd %dh %dm", d, h, m);
        html += "<div class='stat-row'><span class='stat-label'>Uptime</span><span class='stat-val'>" + std::string(ub) + "</span></div>";
    }

    // Load
    std::string loadavg = readProcFile("/proc/loadavg");
    if (!loadavg.empty()) {
        std::istringstream iss(loadavg);
        std::string l1, l5, l15; iss >> l1 >> l5 >> l15;
        html += "<div class='stat-row'><span class='stat-label'>Load 1/5/15</span><span class='stat-val'>" + l1 + " / " + l5 + " / " + l15 + "</span></div>";
    }
    html += "</div>";

    // Card 2: Memory
    html += "<div class='card'><h2>Memory</h2>";
    if (::sysinfo(&si) == 0) {
        double usedPct = 100.0 * (1.0 - (double)si.freeram / si.totalram);
        html += "<div class='stat-row'><span class='stat-label'>Total</span><span class='stat-val'>" + fmtMem(si.totalram) + "</span></div>";
        html += "<div class='stat-row'><span class='stat-label'>Free</span><span class='stat-val'>" + fmtMem(si.freeram) + "</span></div>";
        html += "<div class='stat-row'><span class='stat-label'>Used</span><span class='stat-val'>" + fmtMem(si.totalram - si.freeram) + "</span></div>";
        html += bar(usedPct);
        html += "<div style='font-size:11px;color:#888;margin-top:2px'>" + std::to_string((int)usedPct) + "% used</div>";
    }
    html += "</div>";

    // Card 3: Process
    html += "<div class='card'><h2>Process</h2>";
    std::string status = readProcFile("/proc/self/status");
    auto ff = [&status](const char* k) {
        std::istringstream iss(status); std::string l;
        while (std::getline(iss, l))
            if (l.compare(0, strlen(k), k) == 0) {
                auto p = l.find(':'); if (p != std::string::npos) { const char* v = l.c_str()+p+1; while (*v==' '||*v=='\t')++v; return std::string(v); }
            }
        return std::string("N/A");
    };
    html += "<div class='stat-row'><span class='stat-label'>PID</span><span class='stat-val'>" + std::to_string(getpid()) + "</span></div>";
    html += "<div class='stat-row'><span class='stat-label'>Name</span><span class='stat-val'>" + ff("Name") + "</span></div>";
    html += "<div class='stat-row'><span class='stat-label'>Threads</span><span class='stat-val'>" + ff("Threads") + "</span></div>";
    html += "<div class='stat-row'><span class='stat-label'>RSS</span><span class='stat-val'>" + ff("VmRSS") + " kB</span></div>";
    html += "<div class='stat-row'><span class='stat-label'>VM</span><span class='stat-val'>" + ff("VmSize") + " kB</span></div>";

    // CPU time
    std::string stat = readProcFile("/proc/self/stat");
    auto rp = stat.rfind(')');
    if (rp != std::string::npos) {
        std::istringstream iss(stat.substr(rp+2)); std::string f; int idx=0; long ut=0,st=0;
        while (iss>>f) { if (idx==11) ut=std::stol(f); if (idx==12){st=std::stol(f);break;} ++idx; }
        long tck = sysconf(_SC_CLK_TCK); char cb[32];
        snprintf(cb, sizeof(cb), "%.2f s", (ut+st)/(double)tck);
        html += "<div class='stat-row'><span class='stat-label'>CPU Time</span><span class='stat-val'>" + std::string(cb) + "</span></div>";
    }

    struct rusage usage;
    if (::getrusage(RUSAGE_SELF, &usage) == 0) {
        html += "<div class='stat-row'><span class='stat-label'>Vol Sw</span><span class='stat-val'>" + std::to_string(usage.ru_nvcsw) + "</span></div>";
        html += "<div class='stat-row'><span class='stat-label'>Invol Sw</span><span class='stat-val'>" + std::to_string(usage.ru_nivcsw) + "</span></div>";
    }
    html += "</div>";

    // Card 4: Performance
    html += "<div class='card'><h2>Performance</h2>";
    if (connectionStatsCb_) html += "<div class='stat-row'><span class='stat-label'>Connections</span><span class='stat-val'>" + connectionStatsCb_() + "</span></div>";
    if (requestStatsCb_)    html += "<div class='stat-row'><span class='stat-label'>Requests</span><span class='stat-val'>" + requestStatsCb_() + "</span></div>";
    if (::getrusage(RUSAGE_SELF, &usage) == 0) {
        char ub[32], sb[32];
        snprintf(ub, sizeof(ub), "%.3f s", usage.ru_utime.tv_sec + usage.ru_utime.tv_usec/1e6);
        snprintf(sb, sizeof(sb), "%.3f s", usage.ru_stime.tv_sec + usage.ru_stime.tv_usec/1e6);
        html += "<div class='stat-row'><span class='stat-label'>User CPU</span><span class='stat-val'>" + std::string(ub) + "</span></div>";
        html += "<div class='stat-row'><span class='stat-label'>Sys CPU</span><span class='stat-val'>" + std::string(sb) + "</span></div>";
    }
    html += "</div>";

    html += "</div>"; // grid
    html += htmlFooter();
    resp->setBody(html);
    resp->setContentType("text/html");
}

void Inspector::handleProcess(const HttpRequest&, HttpResponse* resp) {
    std::string html = htmlHeader("Process Info");
    html += "<div class='nav'><a href='/'>Overview</a>"
            "<a href='/system'>System</a><a href='/performance'>Performance</a></div>";

    std::vector<std::pair<std::string, std::string>> rows;
    rows.emplace_back("PID", std::to_string(getpid()));

    std::string status = readProcFile("/proc/self/status");
    auto findField = [&status](const char* key) -> std::string {
        std::istringstream iss(status);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.compare(0, strlen(key), key) == 0) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    const char* val = line.c_str() + pos + 1;
                    while (*val == ' ' || *val == '\t') ++val;
                    return val;
                }
            }
        }
        return "N/A";
    };

    rows.emplace_back("Name", findField("Name"));
    rows.emplace_back("State", findField("State"));
    rows.emplace_back("Threads", findField("Threads"));
    rows.emplace_back("VmSize (kB)", findField("VmSize"));
    rows.emplace_back("VmRSS (kB)", findField("VmRSS"));
    rows.emplace_back("VmData (kB)", findField("VmData"));
    rows.emplace_back("VmStk (kB)", findField("VmStk"));
    rows.emplace_back("FDSize", findField("FDSize"));

    std::string stat = readProcFile("/proc/self/stat");
    auto rparen = stat.rfind(')');
    if (rparen != std::string::npos) {
        std::istringstream iss(stat.substr(rparen + 2));
        std::string field;
        int fieldIdx = 0;
        long utime = 0, stime = 0;
        while (iss >> field) {
            if (fieldIdx == 11) utime = std::stol(field);
            if (fieldIdx == 12) { stime = std::stol(field); break; }
            ++fieldIdx;
        }
        long ticks = sysconf(_SC_CLK_TCK);
        char buf[64];
        snprintf(buf, sizeof(buf), "%.2f s", (utime + stime) / (double)ticks);
        rows.emplace_back("CPU Time", buf);
    }

    struct rusage usage;
    if (::getrusage(RUSAGE_SELF, &usage) == 0) {
        rows.emplace_back("Max RSS (kB)", std::to_string(usage.ru_maxrss));
        rows.emplace_back("Page Reclaims", std::to_string(usage.ru_minflt));
        rows.emplace_back("Page Faults", std::to_string(usage.ru_majflt));
        rows.emplace_back("Vol Ctx Sw", std::to_string(usage.ru_nvcsw));
        rows.emplace_back("Invol Ctx Sw", std::to_string(usage.ru_nivcsw));
    }

    rows.emplace_back("CPU Cores", std::to_string(std::thread::hardware_concurrency()));

    html += "<div class='card'><h2>Process Information</h2>" + kvTable(rows) + "</div>";
    html += htmlFooter();
    resp->setBody(html);
    resp->setContentType("text/html");
}

void Inspector::handleSystem(const HttpRequest&, HttpResponse* resp) {
    std::string html = htmlHeader("System Info");
    html += "<div class='nav'><a href='/'>Overview</a>"
            "<a href='/process'>Process</a><a href='/performance'>Performance</a></div>";

    std::vector<std::pair<std::string, std::string>> rows;

    struct utsname uts;
    if (::uname(&uts) == 0) {
        rows.emplace_back("Hostname", uts.nodename);
        rows.emplace_back("OS", std::string(uts.sysname) + " " + uts.release);
        rows.emplace_back("Architecture", uts.machine);
    }

    rows.emplace_back("CPU Cores", std::to_string(getCpuCount()));

    std::string loadavg = readProcFile("/proc/loadavg");
    if (!loadavg.empty()) {
        std::istringstream iss(loadavg);
        std::string l1, l5, l15;
        iss >> l1 >> l5 >> l15;
        rows.emplace_back("Load (1m)", l1);
        rows.emplace_back("Load (5m)", l5);
        rows.emplace_back("Load (15m)", l15);
    }

    struct sysinfo si;
    if (::sysinfo(&si) == 0) {
        auto fmtMB = [](unsigned long bytes) -> std::string {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
            return buf;
        };
        rows.emplace_back("Total RAM", fmtMB(si.totalram));
        rows.emplace_back("Free RAM", fmtMB(si.freeram));
        rows.emplace_back("Shared RAM", fmtMB(si.sharedram));
        rows.emplace_back("Buffer RAM", fmtMB(si.bufferram));
        rows.emplace_back("Total Swap", fmtMB(si.totalswap));
        rows.emplace_back("Free Swap", fmtMB(si.freeswap));
        rows.emplace_back("Processes", std::to_string(si.procs));

        if (si.totalram > 0) {
            double usedPct = 100.0 * (1.0 - (double)si.freeram / si.totalram);
            char buf[128];
            snprintf(buf, sizeof(buf),
                     "<div class='bar'><div class='bar-fill' style='width:%.1f%%'>"
                     "</div></div><span style='font-size:12px;color:#888'>%.1f%% used</span>",
                     usedPct, usedPct);
            rows.emplace_back("Memory Usage", buf);
        }

        long uptime = si.uptime;
        int days = uptime / 86400;
        int hrs  = (uptime % 86400) / 3600;
        int mins = (uptime % 3600) / 60;
        char buf[64];
        snprintf(buf, sizeof(buf), "%dd %dh %dm", days, hrs, mins);
        rows.emplace_back("Uptime", buf);
    }

    html += "<div class='card'><h2>System Information</h2>" + kvTable(rows) + "</div>";
    html += htmlFooter();
    resp->setBody(html);
    resp->setContentType("text/html");
}

void Inspector::handlePerformance(const HttpRequest&, HttpResponse* resp) {
    std::string html = htmlHeader("Performance");
    html += "<div class='nav'><a href='/'>Overview</a>"
            "<a href='/process'>Process</a><a href='/system'>System</a></div>";

    std::vector<std::pair<std::string, std::string>> rows;

    if (connectionStatsCb_) rows.emplace_back("Connections", connectionStatsCb_());
    if (requestStatsCb_) rows.emplace_back("Total Requests", requestStatsCb_());

    struct rusage usage;
    if (::getrusage(RUSAGE_SELF, &usage) == 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f s", usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6);
        rows.emplace_back("User CPU", buf);
        snprintf(buf, sizeof(buf), "%.3f s", usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6);
        rows.emplace_back("System CPU", buf);
    }

    html += "<div class='card'><h2>Performance Metrics</h2>" + kvTable(rows) + "</div>";
    html += htmlFooter();
    resp->setBody(html);
    resp->setContentType("text/html");
}

std::string Inspector::readProcFile(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::string content;
    content.reserve(4096);
    char buf[4096];
    while (f.read(buf, sizeof(buf)).gcount() > 0) {
        content.append(buf, f.gcount());
    }
    return content;
}

long Inspector::getCpuCount() {
    long n = sysconf(_SC_NPROCESSORS_CONF);
    return n > 0 ? n : 1;
}

std::string Inspector::htmlHeader(const std::string& title) {
    return
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta http-equiv='refresh' content='5'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>" + title + "</title><style>" + std::string(kCss) +
        "</style></head><body><h1>" + title + "</h1>";
}

std::string Inspector::htmlFooter() {
    return "<div style='text-align:center;margin-top:30px;color:#999;font-size:12px'>"
           "muduoSelf Inspector</div></body></html>";
}

std::string Inspector::kvTable(const std::vector<std::pair<std::string, std::string>>& rows) {
    std::string html = "<table><tr><th>Metric</th><th>Value</th></tr>";
    for (const auto& r : rows) {
        html += "<tr><td>" + r.first + "</td><td class='num'>" + r.second + "</td></tr>";
    }
    html += "</table>";
    return html;
}

}  // namespace jpmuduo
