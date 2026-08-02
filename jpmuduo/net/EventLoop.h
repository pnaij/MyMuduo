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

    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);

    TimerId runAt(TimeStamp time, TimerCallback cb);
    TimerId runAfter(double delay, TimerCallback cb);
    TimerId runEvery(double interval, TimerCallback cb);
    void cancel(TimerId timerId);

    void wakeup();

    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead();
    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;
    std::atomic_bool quit_;

    const pid_t threadId_;

    TimeStamp pollReturnTime_;
    std::unique_ptr<Poller> poller_;        //指向对应的poller
    std::unique_ptr<TimerQueue> timerQueue_;

    int wakeupFd_;                          //用来唤醒线程
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_;       //用于表示是否正在执行排队的函数
    std::vector<Functor> pendingFunctors_;
    std::mutex mutex_;
};


}  // namespace jpmuduo

#endif //JPMUDUO_EVENTLOOP_H
