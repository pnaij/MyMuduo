//
// Created by jianp on 2025/12/10.
//

#include "jpmuduo/net/EventLoopThreadPool.h"
#include "jpmuduo/net/EventLoopThread.h"
#include "jpmuduo/base/Logger.h"

#include <memory>
#include <thread>

namespace jpmuduo {

static int computeDefaultThreadNum() {
    int n = static_cast<int>(std::thread::hardware_concurrency());
    if (n <= 0) {
        n = 1;
    }
    return n;
}

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(computeDefaultThreadNum())
    , next_(0) {
    LOG_INFO("EventLoopThreadPool default threads: %d\n", numThreads_);
}

EventLoopThreadPool::~EventLoopThreadPool() {

}

void EventLoopThreadPool::start(const EventLoopThreadPool::ThreadInitCallback &cb) {
    started_ = true;

    for(int i = 0;i < numThreads_; i++) {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop());
    }

    if(numThreads_ == 0 && cb) {
        cb(baseLoop_);
    }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    EventLoop* loop = baseLoop_;

    if(!loops_.empty()) {
        loop = loops_[next_];
        ++next_;
        if(next_ >= loops_.size()) {
            next_ = 0;
        }
    }

    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() {
    if(loops_.empty()) {
        return std::vector<EventLoop*>(1, baseLoop_);
    }else {
        return loops_;
    }
}

}  // namespace jpmuduo