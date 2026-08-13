//
// Created by jianp on 2025/12/9.
//

#include "jpmuduo/net/EventLoopThread.h"
#include "jpmuduo/net/EventLoop.h"

#include <assert.h>

namespace jpmuduo {

EventLoopThread::EventLoopThread(const EventLoopThread::ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb) {

}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if(loop_ != nullptr) {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop() {
    assert(!thread_.started());  // 重复调用会销毁 joinable 线程导致 std::terminate
    thread_.start();

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr) {
            cond_.wait(lock);
        }
        loop = loop_;
    }

    return loop;
}

void EventLoopThread::threadFunc() {
    EventLoop loop;

    if(callback_) {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}

}  // namespace jpmuduo