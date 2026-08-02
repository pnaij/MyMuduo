//
// HttpResponse.h - HTTP response builder
//

#ifndef JPMUDUO_HTTPRESPONSE_H
#define JPMUDUO_HTTPRESPONSE_H

#include "jpmuduo/base/noncopyable.h"

#include <string>
#include <unordered_map>

namespace jpmuduo {

class Buffer;

enum HttpStatusCode {
    kUnknown = 0,
    k100Continue = 100,
    k200Ok = 200,
    k201Created = 201,
    k204NoContent = 204,
    k301MovedPermanently = 301,
    k302Found = 302,
    k304NotModified = 304,
    k400BadRequest = 400,
    k401Unauthorized = 401,
    k403Forbidden = 403,
    k404NotFound = 404,
    k405MethodNotAllowed = 405,
    k408RequestTimeout = 408,
    k413PayloadTooLarge = 413,
    k500InternalServerError = 500,
    k501NotImplemented = 501,
    k502BadGateway = 502,
    k503ServiceUnavailable = 503,
    k505HttpVersionNotSupported = 505,
};

class HttpResponse : noncopyable {
public:
    HttpResponse(bool close)
        : statusCode_(kUnknown), closeConnection_(close) {}

    HttpStatusCode statusCode() const { return statusCode_; }
    const std::string& body() const { return body_; }

    HttpResponse& setStatusCode(HttpStatusCode code) { statusCode_ = code; return *this; }
    HttpResponse& setStatusMessage(const std::string& msg) { statusMessage_ = msg; return *this; }
    HttpResponse& setCloseConnection(bool on) { closeConnection_ = on; return *this; }
    bool closeConnection() const { return closeConnection_; }

    HttpResponse& setContentType(const std::string& ct) { addHeader("Content-Type", ct); return *this; }
    HttpResponse& setBody(const std::string& b) { body_ = b; return *this; }

    HttpResponse& addHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
        return *this;
    }

    void appendToBuffer(Buffer* buf) const;

    static const char* reasonPhrase(HttpStatusCode code);

private:
    HttpStatusCode statusCode_;
    std::string statusMessage_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
    bool closeConnection_;
};

}  // namespace jpmuduo

#endif
