//
// Created by jianp on 2025/12/7.
//

#ifndef JPMUDUO_EVENTLOOP_H
#define JPMUDUO_EVENTLOOP_H

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "jpmuduo/base/noncopyable.h"
#include "jpmuduo/base/TimeStamp.h"
#include "jpmuduo/base/CurrentThread.h"
#include "jpmuduo/net/Timer.h"

namespace jpmuduo {

class Poller;
class Channel;
class TimerQueue;

class EventLoop : noncopyable {
public:
    using Functor = std::function<void()>;
    using TimerCallback = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();        //核心函数
    void quit();

    TimeStamp pollReturnTime() const { return pollReturnTime_; }
    int64_t iteration() const { return iteration_; }

    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);
    size_t queueSize() const;

    TimerId runAt(TimeStamp time, TimerCallback cb);
    TimerId runAfter(double delay, TimerCallback cb);
    TimerId runEvery(double interval, TimerCallback cb);
    void cancel(TimerId timerId);

    void wakeup();

    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    void assertInLoopThread() {
        if (!isInLoopThread()) {
            abortNotInLoopThread();
        }
    }
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
    bool eventHandling() const { return eventHandling_; }

    static EventLoop* getEventLoopOfCurrentThread();

private:
    void abortNotInLoopThread();
    void handleRead();
    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;

    bool looping_;
    std::atomic_bool quit_;
    bool eventHandling_;
    bool callingPendingFunctors_;
    int64_t iteration_;
    const pid_t threadId_;

    TimeStamp pollReturnTime_;
    std::unique_ptr<Poller> poller_;        //指向对应的poller
    std::unique_ptr<TimerQueue> timerQueue_;

    int wakeupFd_;                          //用来唤醒线程
    std::unique_ptr<Channel> wakeupChannel_;

    Channel* currentActiveChannel_;
    ChannelList activeChannels_;

    std::vector<Functor> pendingFunctors_;
    mutable std::mutex mutex_;
};


}  // namespace jpmuduo

#endif //JPMUDUO_EVENTLOOP_H