//
// Created by jianp on 2026/5/16.
//

#include "jpmuduo/net/TimerQueue.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/base/Logging.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <strings.h>
#include <cstdint>

namespace jpmuduo {

int TimerQueue::createTimerfd() {
    int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) {
        LOG_SYSFATAL << "timerfd_create error";
    }
    return fd;
}

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop)
    , timerfd_(createTimerfd())
    , timerfdChannel_(loop, timerfd_)
    , callingExpiredTimers_(false)
{
    timerfdChannel_.setReadCallback(
        std::bind(&TimerQueue::handleRead, this, std::placeholders::_1));
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue() {
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);

    for (const auto& entry : timers_) {
        delete entry.second;
    }
}

TimerId TimerQueue::addTimer(Timer::Callback cb, TimeStamp when, double interval) {
    Timer* timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop([this, timer]() {
        bool earliestChanged = insert(timer);
        if (earliestChanged) {
            resetTimerfd(timer->expiration());
        }
    });
    return TimerId(timer, timer->sequence());
}

void TimerQueue::cancel(TimerId timerId) {
    loop_->runInLoop([this, timerId]() {
        ActiveTimer entry(timerId.timer(), timerId.sequence());
        auto it = activeTimers_.find(entry);
        if (it != activeTimers_.end()) {
            cancelingTimers_.insert(entry);
            activeTimers_.erase(it);
        }
    });
}

void TimerQueue::handleRead(TimeStamp receiveTime) {
    readTimerfd();
    TimeStamp now(TimeStamp::now());

    std::vector<Entry> expired = getExpired(now);

    callingExpiredTimers_ = true;
    for (const auto& entry : expired) {
        Timer* timer = entry.second;
        ActiveTimer active(timer, timer->sequence());
        if (cancelingTimers_.find(active) == cancelingTimers_.end()) {
            timer->run();
        }
    }
    callingExpiredTimers_ = false;

    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(TimeStamp now) {
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    auto end = timers_.lower_bound(sentry);
    std::vector<Entry> expired(timers_.begin(), end);
    timers_.erase(timers_.begin(), end);

    for (const auto& entry : expired) {
        ActiveTimer active(entry.second, entry.second->sequence());
        activeTimers_.erase(active);
    }

    return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, TimeStamp now) {
    for (const auto& entry : expired) {
        Timer* timer = entry.second;
        auto cancelIt = cancelingTimers_.find(ActiveTimer(timer, timer->sequence()));
        if (cancelIt != cancelingTimers_.end()) {
            cancelingTimers_.erase(cancelIt);
            delete timer;
            continue;
        }
        if (timer->repeat()) {
            timer->restart(now);
            insert(timer);
        } else {
            delete timer;
        }
    }

    cancelingTimers_.clear();

    if (!timers_.empty()) {
        resetTimerfd(timers_.begin()->first);
    }
}

bool TimerQueue::insert(Timer* timer) {
    bool earliestChanged = false;
    TimeStamp when = timer->expiration();
    if (timers_.empty() || when < timers_.begin()->first) {
        earliestChanged = true;
    }
    timers_.insert(Entry(when, timer));
    activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
    return earliestChanged;
}

void TimerQueue::resetTimerfd(TimeStamp expiration) {
    struct itimerspec newValue;
    bzero(&newValue, sizeof(newValue));

    int64_t microSeconds = expiration.microSecondsSinceEpoch()
                           - TimeStamp::now().microSecondsSinceEpoch();
    if (microSeconds < 100) {
        microSeconds = 100;
    }

    newValue.it_value.tv_sec = static_cast<time_t>(microSeconds / TimeStamp::kMicroSecondsPerSecond);
    newValue.it_value.tv_nsec = static_cast<long>((microSeconds % TimeStamp::kMicroSecondsPerSecond) * 1000);

    if (::timerfd_settime(timerfd_, 0, &newValue, nullptr) < 0) {
        LOG_SYSERR << "timerfd_settime error";
    }
}

void TimerQueue::readTimerfd() {
    uint64_t howmany;
    ssize_t n = ::read(timerfd_, &howmany, sizeof(howmany));
    if (n != sizeof(howmany)) {
        LOG_ERROR << "TimerQueue::readTimerfd reads " << n << " bytes instead of 8";
    }
}

}  // namespace jpmuduo
