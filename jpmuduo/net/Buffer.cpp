//
// Created by jianp on 2025/12/10.
//

#include "jpmuduo/net/Buffer.h"
#include "jpmuduo/net/SocketsOps.h"

#include <assert.h>
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>
#include <endian.h>
#include <string.h>

namespace jpmuduo {

// ─── Peek (read without consuming) ───────────────────────────────

int64_t Buffer::peekInt64() const {
    int64_t be = 0;
    ::memcpy(&be, peek(), sizeof(be));
    return be64toh(be);
}

int32_t Buffer::peekInt32() const {
    int32_t be = 0;
    ::memcpy(&be, peek(), sizeof(be));
    return be32toh(be);
}

int16_t Buffer::peekInt16() const {
    int16_t be = 0;
    ::memcpy(&be, peek(), sizeof(be));
    return be16toh(be);
}

int8_t Buffer::peekInt8() const {
    return static_cast<int8_t>(*peek());
}

// ─── Peek from prepend region (network byte order) ──────────────

int64_t Buffer::peekPrependInt64() const {
    assert(prependableBytes() >= sizeof(int64_t));
    int64_t be = 0;
    ::memcpy(&be, begin() + readerIndex_, sizeof(be));
    return be64toh(be);
}

int32_t Buffer::peekPrependInt32() const {
    assert(prependableBytes() >= sizeof(int32_t));
    int32_t be = 0;
    ::memcpy(&be, begin() + readerIndex_, sizeof(be));
    return be32toh(be);
}

int16_t Buffer::peekPrependInt16() const {
    assert(prependableBytes() >= sizeof(int16_t));
    int16_t be = 0;
    ::memcpy(&be, begin() + readerIndex_, sizeof(be));
    return be16toh(be);
}

int8_t Buffer::peekPrependInt8() const {
    assert(prependableBytes() >= sizeof(int8_t));
    return static_cast<int8_t>(*(begin() + readerIndex_));
}

// ─── Read (peek + consume) ───────────────────────────────────────

int64_t Buffer::readInt64() {
    int64_t v = peekInt64();
    retrieve(sizeof(v));
    return v;
}

int32_t Buffer::readInt32() {
    int32_t v = peekInt32();
    retrieve(sizeof(v));
    return v;
}

int16_t Buffer::readInt16() {
    int16_t v = peekInt16();
    retrieve(sizeof(v));
    return v;
}

int8_t Buffer::readInt8() {
    int8_t v = peekInt8();
    retrieve(sizeof(v));
    return v;
}

// ─── Prepend (kCheapPrepend space, network byte order) ───────────

void Buffer::prependInt64(int64_t x) {
    int64_t be = htobe64(x);
    prepend(static_cast<const void*>(&be), sizeof(be));
}

void Buffer::prependInt32(int32_t x) {
    int32_t be = htobe32(x);
    prepend(static_cast<const void*>(&be), sizeof(be));
}

void Buffer::prependInt16(int16_t x) {
    int16_t be = htobe16(x);
    prepend(static_cast<const void*>(&be), sizeof(be));
}

void Buffer::prependInt8(int8_t x) {
    prepend(static_cast<const void*>(&x), sizeof(x));
}

// ─── Append (network byte order) ─────────────────────────────────

void Buffer::appendInt64(int64_t x) {
    int64_t be = htobe64(x);
    append(static_cast<const char*>(static_cast<const void*>(&be)), sizeof(be));
}

void Buffer::appendInt32(int32_t x) {
    int32_t be = htobe32(x);
    append(static_cast<const char*>(static_cast<const void*>(&be)), sizeof(be));
}

void Buffer::appendInt16(int16_t x) {
    int16_t be = htobe16(x);
    append(static_cast<const char*>(static_cast<const void*>(&be)), sizeof(be));
}

void Buffer::appendInt8(int8_t x) {
    append(static_cast<const char*>(static_cast<const void*>(&x)), sizeof(x));
}

// ─── Delimiter search ────────────────────────────────────────────

const char* Buffer::findCRLF() const {
    return findCRLF(peek());
}

const char* Buffer::findCRLF(const char* start) const {
    const char* crlf = static_cast<const char*>(::memmem(start, readableBytes() - (start - peek()), "\r\n", 2));
    return crlf;
}

const char* Buffer::findEOL() const {
    return findEOL(peek());
}

const char* Buffer::findEOL(const char* start) const {
    const void* eol = ::memchr(start, '\n', readableBytes() - (start - peek()));
    return static_cast<const char*>(eol);
}

void Buffer::retrieveUntil(const char* end) {
    retrieve(end - peek());
}

// ─── Internal: prepend helper ────────────────────────────────────

void Buffer::prepend(const void* data, size_t len) {
    assert(len <= prependableBytes());
    readerIndex_ -= len;
    ::memcpy(begin() + readerIndex_, data, len);
}

// ─── Shrink ──────────────────────────────────────────────────────

void Buffer::shrink(size_t reserve) {
    Buffer other;
    other.ensureWritableBytes(readableBytes() + reserve);
    other.append(peek(), readableBytes());
    swap(other);
}

// ─── readFd / writeFd ────────────────────────────────────────────

ssize_t Buffer::readFd(int fd, int *saveErrno) {
    char extrabuf[65536] = {0};

    struct iovec vec[2];

    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = sockets::readv(fd, vec, iovcnt);
    if(n < 0) {
        *saveErrno = errno;
    }else if(n <= writable) {
        writerIndex_ += n;
    }else {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno) {
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0) {
        *saveErrno = errno;
    }

    return n;
}

}  // namespace jpmuduo