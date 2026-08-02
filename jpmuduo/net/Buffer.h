//
// Created by jianp on 2025/12/10.
//

#ifndef JPMUDUO_BUFFER_H
#define JPMUDUO_BUFFER_H

#include "jpmuduo/base/noncopyable.h"

#include <vector>
#include <string>
#include <algorithm>

namespace jpmuduo {

class Buffer : noncopyable {
public:
    static const size_t kCheapPrepend = 8;      //buffer前面空出来的空间存一些数据
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend) {

    }

    Buffer(Buffer&& rhs) noexcept
        : buffer_(std::move(rhs.buffer_))
        , readerIndex_(rhs.readerIndex_)
        , writerIndex_(rhs.writerIndex_) {
        rhs.readerIndex_ = kCheapPrepend;
        rhs.writerIndex_ = kCheapPrepend;
    }

    Buffer& operator=(Buffer&& rhs) noexcept {
        if (this != &rhs) {
            buffer_ = std::move(rhs.buffer_);
            readerIndex_ = rhs.readerIndex_;
            writerIndex_ = rhs.writerIndex_;
            rhs.readerIndex_ = kCheapPrepend;
            rhs.writerIndex_ = kCheapPrepend;
        }
        return *this;
    }

    // Swap contents with another buffer (zero-copy)
    void swap(Buffer& rhs) {
        buffer_.swap(rhs.buffer_);
        std::swap(readerIndex_, rhs.readerIndex_);
        std::swap(writerIndex_, rhs.writerIndex_);
    }

    size_t readableBytes() const {//可读空间大小
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const {//可写空间大小
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const {//查看开头有多少数据空间可以利用
        return readerIndex_;
    }

    const char* peek() const {//返回buffer中可读数据的起始地址
        return begin() + readerIndex_;
    }

    void retrieve(size_t len) {//在buffer中取出len长度的数据，此处只移动游标
        if(len < readableBytes()) {
            readerIndex_ += len;
        }else {
            retrieveAll();
        }
    }

    void retrieveAll() {//游标移动
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    std::string retrieveAllAsString() {
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len) {
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    void ensureWritableBytes(size_t len) {//确保空闲可写空间可以写入len长度大小
        if(writableBytes() < len) {
            makeSpace(len);
        }
    }

    void append(const char* data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWirte());
        writerIndex_ += len;
    }

    char* beginWirte() {
        return begin() + writerIndex_;
    }

    const char* beginWirte() const {
        return begin() + writerIndex_;
    }

    ssize_t readFd(int fd, int* saveErrno);
    ssize_t writeFd(int fd, int* saveErrno);

    // --- Framing primitives ---
    // Peek (read without consuming) from readable region
    int64_t peekInt64() const;
    int32_t peekInt32() const;
    int16_t peekInt16() const;
    int8_t  peekInt8() const;

    // Read (peek + consume) from readable region
    int64_t readInt64();
    int32_t readInt32();
    int16_t readInt16();
    int8_t  readInt8();

    // Prepend in network byte order (write into prepend region)
    void prependInt64(int64_t x);
    void prependInt32(int32_t x);
    void prependInt16(int16_t x);
    void prependInt8(int8_t x);

    // Peek from prepend region (network byte order, most recently prepended first)
    int64_t peekPrependInt64() const;
    int32_t peekPrependInt32() const;
    int16_t peekPrependInt16() const;
    int8_t  peekPrependInt8() const;

    // Append in network byte order
    void appendInt64(int64_t x);
    void appendInt32(int32_t x);
    void appendInt16(int16_t x);
    void appendInt8(int8_t x);

    // Delimiter search
    const char* findCRLF() const;
    const char* findCRLF(const char* start) const;
    const char* findEOL() const;
    const char* findEOL(const char* start) const;

    // Retrieve until a pointer in the readable region
    void retrieveUntil(const char* end);

    // Prepend data before current readable region (max prependableBytes() bytes)
    void prepend(const void* data, size_t len);


    // Total capacity of the underlying buffer
    size_t internalCapacity() const { return buffer_.capacity(); }

    // Shrink internal buffer, keeping at least |reserve| bytes
    void shrink(size_t reserve);

    bool empty() const { return readableBytes() == 0; }

private:
    char* begin() {
        return &*buffer_.begin();
    }

    const char* begin() const {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len) {
        if(writableBytes() + prependableBytes() < len + kCheapPrepend) {
            buffer_.resize(writerIndex_ + len);
        }else {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;

    //用两个游标来计算读写空间大小
    size_t readerIndex_;
    size_t writerIndex_;
};


inline void swap(Buffer& a, Buffer& b) { a.swap(b); }

}  // namespace jpmuduo

#endif //JPMUDUO_BUFFER_H
