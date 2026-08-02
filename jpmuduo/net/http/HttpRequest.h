//
// HttpRequest.h - parsed HTTP request
//

#ifndef JPMUDUO_HTTPREQUEST_H
#define JPMUDUO_HTTPREQUEST_H

#include "jpmuduo/base/noncopyable.h"

#include <string>
#include <unordered_map>
#include <algorithm>

namespace jpmuduo {

enum HttpMethod {
    kInvalid, kGet, kPost, kHead, kPut, kDelete,
    kOptions, kPatch, kTrace, kConnect
};

class HttpRequest : noncopyable {
public:
    HttpRequest() : method_(kInvalid) {}
    HttpRequest(HttpRequest&&) = default;
    HttpRequest& operator=(HttpRequest&&) = default;

    HttpMethod method() const { return method_; }
    const std::string& path() const { return path_; }
    const std::string& query() const { return query_; }

    void setMethod(HttpMethod m) { method_ = m; }
    void setPath(const char* start, const char* end) { path_.assign(start, end); }
    void setQuery(const char* start, const char* end) {
        query_.assign(start, end);
    }

    void addHeader(const char* start, const char* colon, const char* end);
    std::string getHeader(const std::string& field) const;

    void setBody(const std::string& b) { body_ = b; }
    const std::string& body() const { return body_; }

    // called after parsing is complete (sets method enum from string)
    bool setMethodString(const char* start, const char* end);

    void swap(HttpRequest& that);

private:
    HttpMethod method_;
    std::string path_;
    std::string query_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
};

}  // namespace jpmuduo

#endif
