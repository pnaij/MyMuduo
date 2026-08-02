//
// Created by jianp on 2026/5/16.
//

#ifndef JPMUDUO_TIMER_H
#define JPMUDUO_TIMER_H

#include "jpmuduo/base/TimeStamp.h"
#include "jpmuduo/base/noncopyable.h"

#include <functional>
#include <atomic>

namespace jpmuduo {

class Timer : noncopyable {
public:
    using Callback = std::function<void()>;

    Timer(Callback cb, TimeStamp when, double interval)
        : callback_(std::move(cb))
        , expiration_(when)
        , interval_(interval)
        , repeat_(interval > 0.0)
        , sequence_(nextSequence())
    {}

    void run() const { if (callback_) callback_(); }

    TimeStamp expiration() const { return expiration_; }
    bool repeat() const { return repeat_; }
    int64_t sequence() const { return sequence_; }

    void restart(TimeStamp now) {
        if (repeat_) {
            expiration_ = TimeStamp::addTime(now, interval_);
        } else {
            expiration_ = TimeStamp();
        }
    }

    static int64_t nextSequence() {
        static std::atomic<int64_t> seq(0);
        return ++seq;
    }

private:
    Callback callback_;
    TimeStamp expiration_;
    double interval_;
    bool repeat_;
    int64_t sequence_;
};

class TimerId {
public:
    TimerId() : timer_(nullptr), sequence_(0) {}
    TimerId(Timer* timer, int64_t seq) : timer_(timer), sequence_(seq) {}

    Timer* timer() const { return timer_; }
    int64_t sequence() const { return sequence_; }

private:
    Timer* timer_;
    int64_t sequence_;
};

}  // namespace jpmuduo

#endif // JPMUDUO_TIMER_H
