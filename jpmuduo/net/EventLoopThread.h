//
// Created by jianp on 2025/12/9.
//

#ifndef JPMUDUO_EVENTLOOPTHREAD_H
#define JPMUDUO_EVENTLOOPTHREAD_H

#include "jpmuduo/base/noncopyable.h"
#include "jpmuduo/base/Thread.h"

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

namespace jpmuduo {

class EventLoop;

class EventLoopThread : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();
private:
    void threadFunc();

    EventLoop *loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};


}  // namespace jpmuduo

#endif //JPMUDUO_EVENTLOOPTHREAD_H
