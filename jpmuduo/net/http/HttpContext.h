//
// HttpContext.h - per-connection incremental HTTP request parser
//

#ifndef JPMUDUO_HTTPCONTEXT_H
#define JPMUDUO_HTTPCONTEXT_H

#include "jpmuduo/base/noncopyable.h"
#include "jpmuduo/net/http/HttpRequest.h"

namespace jpmuduo {

class Buffer;

class HttpContext : noncopyable {
public:
    enum ParseState {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll,
        kInvalid,
    };

    HttpContext() : state_(kExpectRequestLine), parseError_(false) {}
    HttpContext(HttpContext&&) = default;
    HttpContext& operator=(HttpContext&&) = default;

    bool hasError() const { return parseError_; }

    // return true when a complete request has been parsed
    bool parseRequest(Buffer* buf);

    bool gotAll() const { return state_ == kGotAll; }

    void reset() {
        state_ = kExpectRequestLine;
        HttpRequest dummy;
        request_.swap(dummy);
    }

    const HttpRequest& request() const { return request_; }
    HttpRequest& request() { return request_; }

private:
    bool parseRequestLine(const char* begin, const char* end);
    bool parseHeaders(Buffer* buf);
    bool parseBody(Buffer* buf);

    ParseState state_;
    HttpRequest request_;
    int contentLength_;
    bool parseError_;
};

}  // namespace jpmuduo

#endif
