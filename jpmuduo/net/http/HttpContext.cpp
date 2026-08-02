//
// HttpContext.cpp - HTTP request incremental parser
//

#include "jpmuduo/net/http/HttpContext.h"
#include "jpmuduo/net/Buffer.h"
#include "jpmuduo/base/Logger.h"

#include <algorithm>
#include <cstdlib>

namespace jpmuduo {

// ── Main entry: feed buffer, return true when one request is complete ──

bool HttpContext::parseRequest(Buffer* buf) {
    bool hasMore = true;
    while (hasMore) {
        switch (state_) {
        case kExpectRequestLine: {
            const char* crlf = buf->findCRLF();
            if (crlf) {
                if (!parseRequestLine(buf->peek(), crlf)) {
                    return false;  // bad request
                }
                buf->retrieveUntil(crlf);
                buf->retrieve(2);  // skip \r\n
                state_ = kExpectHeaders;
            } else {
                hasMore = false;
            }
            break;
        }
        case kExpectHeaders: {
            if (!parseHeaders(buf)) {
                hasMore = false;   // need more data
            }
            break;
        }
        case kExpectBody: {
            if (!parseBody(buf)) {
                hasMore = false;   // need more data
            }
            break;
        }
        case kGotAll:
            hasMore = false;
            break;
        }
    }
    return state_ == kGotAll;
}

// ── Request line: "GET /path?q=1 HTTP/1.1\r\n" ──

bool HttpContext::parseRequestLine(const char* begin, const char* end) {
    const char* start = begin;
    const char* sp1 = std::find(start, end, ' ');
    if (sp1 == end) return false;

    if (!request_.setMethodString(start, sp1)) {
        parseError_ = true;
        return false;
    }

    start = sp1 + 1;
    const char* sp2 = std::find(start, end, ' ');
    if (sp2 == end) return false;

    const char* question = std::find(start, sp2, '?');
    if (question != sp2) {
        request_.setPath(start, question);
        request_.setQuery(question + 1, sp2);
    } else {
        request_.setPath(start, sp2);
    }

    // Skip HTTP/1.x version (not stored)
    return true;
}

// ── Headers: parse until empty line (\r\n alone) ──

bool HttpContext::parseHeaders(Buffer* buf) {
    const char* start = buf->peek();
    const char* crlf = buf->findCRLF();
    if (!crlf) return false;

    if (crlf == start) {
        // empty line → headers done
        buf->retrieve(2);
        std::string contentLen = request_.getHeader("content-length");
        if (!contentLen.empty()) {
            char* end = nullptr;
            long len = std::strtol(contentLen.c_str(), &end, 10);
            if (end == contentLen.c_str() || *end != '\0' || len < 0) {
                parseError_ = true;
                state_ = kInvalid;
                return false;
            }
            contentLength_ = static_cast<int>(len);
            state_ = kExpectBody;
        } else {
            state_ = kGotAll;
        }
        return true;
    }

    const char* colon = std::find(start, crlf, ':');
    if (colon != crlf) {
        request_.addHeader(start, colon, crlf);
    }

    buf->retrieveUntil(crlf);
    buf->retrieve(2);
    return true;  // keep parsing in the while loop
}

// ── Body: read Content-Length bytes ──

bool HttpContext::parseBody(Buffer* buf) {
    if (buf->readableBytes() < static_cast<size_t>(contentLength_)) {
        return false;
    }
    request_.setBody(std::string(buf->peek(), contentLength_));
    buf->retrieve(contentLength_);
    state_ = kGotAll;
    return true;
}

}  // namespace jpmuduo
