//
// Created by jianp on 2025/11/21.
//

#include "jpmuduo/net/Channel.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/base/Logging.h"

#include <assert.h>
#include <poll.h>
#include <sstream>

namespace jpmuduo {

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop *loop, int fd) : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1),
                                            logHup_(true), tied_(false), eventHandling_(false), addedToLoop_(false) {

}

Channel::~Channel() {
    assert(!eventHandling_);
    assert(!addedToLoop_);
    if (loop_->isInLoopThread()) {
        assert(!loop_->hasChannel(this));
    }
}

void Channel::handleEvent(TimeStamp receiveTime) {
    std::shared_ptr<void> guard;
    if(tied_) {
        guard = tie_.lock();      //通过对tie_提权，如果能获取成功说明对应的connection对象还没被析构
        if(guard) {
            handleEventWithGuard(receiveTime);
        }
    }else {     //这里主要用来处理不属于tcpconnection的channel
        handleEventWithGuard(receiveTime);
    }
}

void Channel::tie(const std::shared_ptr<void>& obj) {
    tie_ = obj;
    tied_ = true;
}

void Channel::remove() {
    assert(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

void Channel::update() {
    addedToLoop_ = true;
    loop_->updateChannel(this);
}

void Channel::handleEventWithGuard(TimeStamp receiveTime) {
    eventHandling_ = true;
    LOG_DEBUG << "channel handleEvent revents:" << reventsToString();

    if((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
        if(logHup_) {
            LOG_WARN << "fd = " << fd_ << " Channel::handleEvent() POLLHUP";
        }
        if(closeCallback_) {
            closeCallback_();
        }
    }

    if(revents_ & POLLNVAL) {
        LOG_WARN << "fd = " << fd_ << " Channel::handleEvent() POLLNVAL";
    }

    if(revents_ & (POLLERR | POLLNVAL)) {
        if(errorCallback_) {
            errorCallback_();
        }
    }

    if(revents_ & (POLLIN | POLLPRI | POLLRDHUP)) {
        if(readCallback_) {
            readCallback_(receiveTime);
        }
    }

    if(revents_ & POLLOUT) {
        if(writeCallback_) {
            writeCallback_();
        }
    }
    eventHandling_ = false;
}

std::string Channel::reventsToString() const {
    return eventsToString(fd_, revents_);
}

std::string Channel::eventsToString() const {
    return eventsToString(fd_, events_);
}

std::string Channel::eventsToString(int fd, int ev) {
    std::ostringstream oss;
    oss << fd << ": ";
    if (ev & POLLIN)
        oss << "IN ";
    if (ev & POLLPRI)
        oss << "PRI ";
    if (ev & POLLOUT)
        oss << "OUT ";
    if (ev & POLLHUP)
        oss << "HUP ";
    if (ev & POLLRDHUP)
        oss << "RDHUP ";
    if (ev & POLLERR)
        oss << "ERR ";
    if (ev & POLLNVAL)
        oss << "NVAL ";

    return oss.str();
}

}  // namespace jpmuduo
