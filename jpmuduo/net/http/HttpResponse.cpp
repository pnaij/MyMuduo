//
// HttpResponse.cpp
//

#include "jpmuduo/net/http/HttpResponse.h"
#include "jpmuduo/net/Buffer.h"
#include <cstdio>
#include <cstring>
#include <ctime>

namespace jpmuduo {

const char* HttpResponse::reasonPhrase(HttpStatusCode code) {
    switch (code) {
    case k100Continue: return "Continue";
    case k200Ok: return "OK";
    case k201Created: return "Created";
    case k204NoContent: return "No Content";
    case k301MovedPermanently: return "Moved Permanently";
    case k302Found: return "Found";
    case k304NotModified: return "Not Modified";
    case k400BadRequest: return "Bad Request";
    case k401Unauthorized: return "Unauthorized";
    case k403Forbidden: return "Forbidden";
    case k404NotFound: return "Not Found";
    case k405MethodNotAllowed: return "Method Not Allowed";
    case k408RequestTimeout: return "Request Timeout";
    case k413PayloadTooLarge: return "Payload Too Large";
    case k500InternalServerError: return "Internal Server Error";
    case k501NotImplemented: return "Not Implemented";
    case k502BadGateway: return "Bad Gateway";
    case k503ServiceUnavailable: return "Service Unavailable";
    case k505HttpVersionNotSupported: return "HTTP Version Not Supported";
    default: return "Unknown";
    }
}

void HttpResponse::appendToBuffer(Buffer* buf) const {
    char line[256];

    // Status line
    snprintf(line, sizeof(line), "HTTP/1.1 %d %s\r\n",
             static_cast<int>(statusCode_), reasonPhrase(statusCode_));
    buf->append(line, strlen(line));

    // Connection
    if (closeConnection_) {
        buf->append("Connection: close\r\n", 19);
    } else {
        buf->append("Connection: Keep-Alive\r\n", 24);
    }

    // Date (RFC 1123)
    time_t now = time(nullptr);
    struct tm gm;
    gmtime_r(&now, &gm);
    char dateBuf[64];
    strftime(dateBuf, sizeof(dateBuf), "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", &gm);
    buf->append(dateBuf, strlen(dateBuf));

    // Server
    buf->append("Server: muduoSelf/1.0\r\n", 23);

    // Custom headers
    for (const auto& h : headers_) {
        buf->append(h.first.data(), h.first.size());
        buf->append(": ", 2);
        buf->append(h.second.data(), h.second.size());
        buf->append("\r\n", 2);
    }

    // Content-Length (skip for 204/304 if no body)
    if (!body_.empty() || (statusCode_ != k204NoContent && statusCode_ != k304NotModified)) {
        snprintf(line, sizeof(line), "Content-Length: %zu\r\n", body_.size());
        buf->append(line, strlen(line));
    }

    // Blank line
    buf->append("\r\n", 2);

    // Body
    if (!body_.empty()) {
        buf->append(body_.data(), body_.size());
    }
}

}  // namespace jpmuduo
