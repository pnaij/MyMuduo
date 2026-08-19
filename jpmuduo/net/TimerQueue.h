//
// Created by jianp on 2026/5/16.
//

#ifndef JPMUDUO_TIMERQUEUE_H
#define JPMUDUO_TIMERQUEUE_H

#include "jpmuduo/net/Timer.h"
#include "jpmuduo/net/TimerId.h"
#include "jpmuduo/net/Channel.h"
#include "jpmuduo/base/noncopyable.h"

#include <set>
#include <vector>

namespace jpmuduo {

class EventLoop;

class TimerQueue : noncopyable {
public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    TimerId addTimer(Timer::Callback cb, TimeStamp when, double interval);
    void cancel(TimerId timerId);

private:
    using Entry = std::pair<TimeStamp, Timer*>;
    using TimerSet = std::set<Entry>;
    using ActiveTimer = std::pair<Timer*, int64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;

    void handleRead(TimeStamp receiveTime);
    std::vector<Entry> getExpired(TimeStamp now);
    void reset(const std::vector<Entry>& expired, TimeStamp now);
    bool insert(Timer* timer);

    void resetTimerfd(TimeStamp expiration);
    void readTimerfd();
    static int createTimerfd();

    EventLoop* loop_;
    const int timerfd_;
    Channel timerfdChannel_;
    TimerSet timers_;
    ActiveTimerSet activeTimers_;
    ActiveTimerSet cancelingTimers_;
    bool callingExpiredTimers_;
};

}  // namespace jpmuduo

#endif // JPMUDUO_TIMERQUEUE_H
