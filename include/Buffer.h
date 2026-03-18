//
// Created by jianp on 2025/12/10.
//

#ifndef MUDUOSELF_BUFFER_H
#define MUDUOSELF_BUFFER_H

#include <vector>
#include <string>
#include <algorithm>

class Buffer {
public:
    static const size_t kCheapPrepend = 8;      //buffer前面空出来的空间存一些数据
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend) {

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


#endif //MUDUOSELF_BUFFER_H
