//
// Created by jianp on 2025/12/7.
//

#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/base/Logging.h"
#include "jpmuduo/net/Poller.h"
#include "jpmuduo/net/Channel.h"
#include "jpmuduo/net/TimerQueue.h"

#include <assert.h>
#include <signal.h>

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>
#include <algorithm>

namespace jpmuduo {

namespace {

__thread EventLoop *t_loopInThisThread = nullptr;

const int kPollTimeMs = 10000;

int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0) {
        LOG_SYSFATAL << "eventfd error";
    }
    return evtfd;
}

#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe {
public:
    IgnoreSigPipe() {
        ::signal(SIGPIPE, SIG_IGN);
    }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj;

}  // unnamed namespace

EventLoop* EventLoop::getEventLoopOfCurrentThread() {
    return t_loopInThisThread;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , eventHandling_(false)
    , callingPendingFunctors_(false)
    , iteration_(0)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , timerQueue_(new TimerQueue(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
    , currentActiveChannel_(nullptr) {
    LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
    if(t_loopInThisThread) {
        LOG_FATAL << "Another EventLoop " << t_loopInThisThread << " exists in this thread " << threadId_;
    }else {
        t_loopInThisThread = this;
    }

    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_
              << " destructs in thread " << CurrentThread::tid();
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;

    LOG_INFO << "EventLoop " << this << " start looping";

    while(!quit_) {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        ++iteration_;
        eventHandling_ = true;
        for(Channel* channel : activeChannels_) {
            currentActiveChannel_ = channel;
            channel->handleEvent(pollReturnTime_);
        }
        currentActiveChannel_ = nullptr;
        eventHandling_ = false;
        doPendingFunctors();
    }

    LOG_INFO << "EventLoop " << this << " stop looping.";
    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    if(!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(EventLoop::Functor cb) {
    if(isInLoopThread()) {
        cb();
    }else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(EventLoop::Functor cb) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(std::move(cb));
    }

    if(!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

size_t EventLoop::queueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pendingFunctors_.size();
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)) {
        LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}

void EventLoop::wakeup() {
    u_int64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)) {
        LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

TimerId EventLoop::runAt(TimeStamp time, TimerCallback cb) {
    return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

TimerId EventLoop::runAfter(double delay, TimerCallback cb) {
    TimeStamp when = TimeStamp::addTime(TimeStamp::now(), delay);
    return timerQueue_->addTimer(std::move(cb), when, 0.0);
}

TimerId EventLoop::runEvery(double interval, TimerCallback cb) {
    TimeStamp when = TimeStamp::addTime(TimeStamp::now(), interval);
    return timerQueue_->addTimer(std::move(cb), when, interval);
}

void EventLoop::cancel(TimerId timerId) {
    timerQueue_->cancel(timerId);
}

void EventLoop::updateChannel(Channel *channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    if (eventHandling_) {
        assert(currentActiveChannel_ == channel ||
               std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
    }
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor& functor : functors) {
        functor();
    }

    callingPendingFunctors_ = false;
}

void EventLoop::abortNotInLoopThread() {
    LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
              << " was created in threadId_ = " << threadId_
              << ", current thread id = " << CurrentThread::tid();
}

}  // namespace jpmuduo