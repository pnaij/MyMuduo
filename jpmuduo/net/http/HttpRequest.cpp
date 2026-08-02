//
// HttpRequest.cpp
//

#include "jpmuduo/net/http/HttpRequest.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace jpmuduo {

void HttpRequest::addHeader(const char* start, const char* colon, const char* end) {
    const char* p = colon;
    while (p > start && *(p - 1) == ' ') --p;    // trim space before ':'
    std::string field(start, p);

    ++colon;                                       // skip ':'
    while (colon < end && *colon == ' ') ++colon;  // trim space after ':'
    std::string value(colon, end);

    for (auto& c : field) c = std::tolower(c);
    headers_[field] = value;
}

std::string HttpRequest::getHeader(const std::string& field) const {
    std::string lowerField = field;
    for (auto& c : lowerField) c = std::tolower(c);
    auto it = headers_.find(lowerField);
    return it != headers_.end() ? it->second : std::string();
}

bool HttpRequest::setMethodString(const char* start, const char* end) {
    std::string m(start, end);
    if (m == "GET")         method_ = kGet;
    else if (m == "POST")   method_ = kPost;
    else if (m == "HEAD")   method_ = kHead;
    else if (m == "PUT")    method_ = kPut;
    else if (m == "DELETE") method_ = kDelete;
    else if (m == "OPTIONS") method_ = kOptions;
    else if (m == "PATCH")   method_ = kPatch;
    else if (m == "TRACE")   method_ = kTrace;
    else if (m == "CONNECT") method_ = kConnect;
    else { method_ = kInvalid; return false; }
    return true;
}

void HttpRequest::swap(HttpRequest& that) {
    std::swap(method_, that.method_);
    path_.swap(that.path_);
    query_.swap(that.query_);
    body_.swap(that.body_);
    headers_.swap(that.headers_);
}

}  // namespace jpmuduo
