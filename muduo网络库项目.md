## 主从Reactor模型

​	在多线程模型中，我们提到，其主要缺陷在于同一时间无法处理**大量新连接**、**IO就绪事件**；因此，将主从模式应用到这一块，就可以解决这个问题。

主从 Reactor 模式中，分为了主 Reactor 和 从 Reactor，分别处理 `新建立的连接`、`IO读写事件/事件分发`。

- 一来，主 Reactor 可以解决同一时间大量新连接，将其注册到从 Reactor 上进行IO事件监听处理
- 二来，IO事件监听相对新连接处理更加耗时，此处我们可以考虑使用线程池来处理。这样能充分利用多核 CPU 的特性，能使更多就绪的IO事件及时处理。

​	简言之，主从多线程模型由多个 Reactor 线程组成，每个 Reactor 线程都有独立的 Selector 对象。MainReactor 仅负责处理客户端连接的 Accept 事件，连接建立成功后将新创建的连接对象注册至 SubReactor。再由 SubReactor 分配线程池中的 I/O 线程与其连接绑定，它将负责连接生命周期内所有的 I/O 事件。

在海量客户端并发请求的场景下，主从多线程模式甚至可以适当增加 SubReactor 线程的数量，从而利用多核能力提升系统的吞吐量。

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.28.3)
project(muduoSelf)

set(LIBRARY_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

aux_source_directory(. SRC_LIST)

add_library(muduoSelf SHARED ${SRC_LIST})
```

## noncopyable代码

```c++
//
// Created by jianp on 2025/11/7.
//

#pragma once
#ifndef MUDUOSELF_NONCOPYABLE_H
#define MUDUOSELF_NONCOPYABLE_H

class noncopyable {
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};

#endif //MUDUOSELF_NONCOPYABLE_H

```

让某些类来继承noncopyable来显示提醒该类不能被拷贝和赋值

## Logger日志代码

### Logger.h

```c++
//
// Created by jianp on 2025/11/7.
//

#ifndef MUDUOSELF_LOGGER_H
#define MUDUOSELF_LOGGER_H

#include <string.h>
#include "noncopyable.h"

#define  LOG_INFO(logmsgFormat, ...) \
    do                               \
    {                                \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(INFO);    \
        char buf[1024] = {0};        \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);\
    } while(0)

#define  LOG_ERROR(logmsgFormat, ...) \
    do                               \
    {                                \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(ERROR);    \
        char buf[1024] = {0};        \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);\
    } while(0)

#define  LOG_FATAL(logmsgFormat, ...) \
    do                               \
    {                                \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(FATAL);    \
        char buf[1024] = {0};        \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);\
    } while(0)


#ifdef MUDEBUG
#define  LOG_DEBUG(logmsgFormat, ...) \
    do                               \
    {                                \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(DEBUG);    \
        char buf[1024] = {0};        \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);\
    } while(0)
#else
    #define LOG_DEBUG(logmsgFormat, ...)
#endif

enum LogLevel {
    INFO,
    ERROR,
    FATAL,
    DEBUG,
};

class Logger : noncopyable {
public:
    static Logger* instance();

    void setLogLevel(int level);

    void log(std::string msg);
private:
    int logLevel_;
    Logger() {}
};


#endif //MUDUOSELF_LOGGER_H

```

### Logger.cpp

```c++
//
// Created by jianp on 2025/11/7.
//

#include <iostream>

#include "TimeStamp.h"
#include "Logger.h"

Logger* Logger::instance() {
    static Logger logger;
    return &logger;
}

void Logger::setLogLevel(int level) {
    logLevel_ = level;
}

void Logger::log(std::string msg) {
    switch (logLevel_) {
        case INFO:
            std::cout << "[INFO]";
            break;
        case ERROR:
            std::cout << "[ERROR]";
            break;
        case FATAL:
            std::cout << "[FATAL]";
            break;
        case DEBUG:
            std::cout << "[DEBUG]";
            break;
        default:
            break;
    }

    std::cout << TimeStamp::now().toString() << " : " << msg << std::endl;
}
```

Logger类中主要就是学到了利用define+VA_ARGS来实现不同格式的信息打印，还有对于Time时间类的了解。

Logger类中得到logger对象采用单例模式，他把构造函数列为私有，通过instance方法进行获得对象。

## Timestamp时间代码

### TimeStamp.h

```c++
//
// Created by jianp on 2025/11/8.
//

#ifndef MUDUOSELF_TIMESTAMP_H
#define MUDUOSELF_TIMESTAMP_H

#include <iostream>
#include <string>
#include <cstdint>

class TimeStamp {
public:
    TimeStamp();
    explicit TimeStamp(int64_t microSecondsSinceEpoch);
    static TimeStamp now();
    std::string toString() const;

private:
    int64_t microSecondsSinceEpoch_;
};


#endif //MUDUOSELF_TIMESTAMP_H

```

### TimeStamp.cpp

```c++
//
// Created by jianp on 2025/11/8.
//

#include <time.h>
#include "TimeStamp.h"

TimeStamp::TimeStamp() : microSecondsSinceEpoch_(0) {}

TimeStamp::TimeStamp(int64_t microSecondsSinceEpoch)
    : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

TimeStamp TimeStamp::now() {
    return TimeStamp(time(NULL));
}

std::string TimeStamp::toString() const {
    char buf[128] = {0};
    tm *tm_time = localtime(&microSecondsSinceEpoch_);
    snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d",
             tm_time->tm_year + 1900,
             tm_time->tm_mon + 1,
             tm_time->tm_mday,
             tm_time->tm_hour,
             tm_time->tm_min,
             tm_time->tm_sec);

    return buf;
}
```

time(NULL)返回从1970.1.1开始的一个64位整数。

localtime将一个上面得到的整数转为一个结构体得到年月日等信息。

## InetAddress代码

InetAddress代码主要就是对socket中的ip和port进行封装，真正对于套接字的一系列操作放在Socekt中去实现。

### InetAddress.h

```c++
//
// Created by jianp on 2025/11/8.
//

#ifndef MUDUOSELF_INETADDRESS_H
#define MUDUOSELF_INETADDRESS_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

class InetAddress {
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr) : addr_(addr) {}

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getSockAddr() const { return &addr_; }
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }

private:
    sockaddr_in addr_;
};


#endif //MUDUOSELF_INETADDRESS_H

```

### InetAddress.cpp

```c++
//
// Created by jianp on 2025/11/8.
//

#include "InetAddress.h"

#include <strings.h>
#include <string.h>

InetAddress::InetAddress(uint16_t port, std::string ip) {
    bzero(&addr_, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}

std::string InetAddress::toIp() const {
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));

    return buf;
}
std::string InetAddress::toIpPort() const {
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    size_t  end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf + end, ":%u", port);

    return buf;
}
uint16_t InetAddress::toPort() const {
    return ntohs(addr_.sin_port);
}
```

## Channel代码



### Channel.h

```c++
//
// Created by jianp on 2025/11/21.
//

#ifndef MUDUOSELF_CHANNEL_H
#define MUDUOSELF_CHANNEL_H

#include "noncopyable.h"
#include "TimeStamp.h"
#include <functional>
#include <memory>

class EventLoop;

class Channel : noncopyable {
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(TimeStamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    void handleEvent(TimeStamp receiveTime);

    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }
    int events() const { return events_; }
    int set_revents(int revt) { revents_ = revt; }

    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    bool isNoneEvent() { return events_ == kNoneEvent; }
    bool isWriting() { return events_ & kWriteEvent; }
    bool isReading() { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    EventLoop* ownerloop() { return loop_; }
    void remove();
private:
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;
    const int fd_;
    int events_;	//设置在fd上的感兴趣事件
    int revents_;	//实际发生在fd上的事件
    int index_;		
    /*channel部分的index是用来表达该channel的状态的，kNew，kAdded，kDeleted，他们分别表示channel还没添加进poller中，channel已经被添加进poller中，将channel打上删除标记但还没实际删除。*/

    //tie_是用来绑定该channel对应的TcpConnection连接的，确保连接还存在
    std::weak_ptr<void> tie_;
    bool tied_;

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

    void update();
    void handleEventWithGuard(TimeStamp receiveTime);
};


#endif //MUDUOSELF_CHANNEL_H

```

从Channel的变量声明中可以看出，Channel这部分连接了EventLoop，它可以从fd_感知发生的事件然后给eventloop进行处理。

### Channel.cpp

```c++
//
// Created by jianp on 2025/11/21.
//

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd) : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false) {

}

Channel::~Channel() {

}

void Channel::handleEvent(TimeStamp receiveTime) {//这里没搞明白为什么要判断一下
    if(tied_) {
        std::shared_ptr<void> guard = tie_.lock();
        if(guard) {
            handleEventWithGuard(receiveTime);
        }
    }else {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::tie(const std::shared_ptr<void>& obj) {
    tie_ = obj;
    tied_ = true;
}

void Channel::remove() {
    loop_->removeChannel(this);
}

void Channel::update() {
    loop_->updateChannel(this);
}

void Channel::handleEventWithGuard(TimeStamp receiveTime) {//对不同事件执行不同的回调操作
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if(closeCallback_) {
            closeCallback_();
        }
    }

    if(revents_ & EPOLLERR) {
        if(errorCallback_) {
            errorCallback_();
        }
    }

    if(revents_ & (EPOLLIN | EPOLLPRI)) {
        if(readCallback_) {
            readCallback_(receiveTime);
        }
    }

    if(revents_ & EPOLLOUT) {
        if(writeCallback_) {
            writeCallback_();
        }
    }
}
```

## Poller抽象层代码

### Poller.h

```c++
//
// Created by jianp on 2025/11/21.
//

#ifndef MUDUOSELF_POLLER_H
#define MUDUOSELF_POLLER_H

#include "noncopyable.h"
#include "TimeStamp.h"
#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

class Poller : noncopyable {//这是一个Poller的抽象类
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller() = default;

    virtual TimeStamp poll(int timeOutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    bool hasChannel(Channel *channel) const;

    static Poller* newDefaultPoller(EventLoop *loop);
protected:
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;
private:
    EventLoop *ownerLoop_;
};


#endif //MUDUOSELF_POLLER_H

```

### Poller.cpp

```c++
//
// Created by jianp on 2025/11/21.
//

#include "Poller.h"
#include "Channel.h"

Poller::Poller(EventLoop *loop) : ownerLoop_(loop) {}

bool Poller::hasChannel(Channel *channel) const {
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}
```

Poll是一个抽象类，可以实现具体的EPoll之类的IO复用Poller

## EPollPoller事件分发器代码

EPollPoller就是一个使用了epoll技术的一个具体实现，它可以监听给它的一系列fd，然后通过对应的Channel通知相应的EventLoop对发生事件的fd进行处理。

### EPollPoller.h

```c++
//
// Created by jianp on 2025/11/21.
//

#ifndef MUDUOSELF_EPollPoller_H
#define MUDUOSELF_EPollPoller_H

#include "Poller.h"
#include "TimeStamp.h"
#include <vector>
#include <sys/epoll.h>

class Channel;

class EPollPoller : public Poller{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    //重写基类的抽象方法
    TimeStamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;
private:
    static const int kInitEventListSize = 16;

    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    void update(int operation, Channel *channel);
    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};


#endif //MUDUOSELF_EPollPoller_H

```

### EPollPoller.cpp

```c++
//
// Created by jianp on 2025/11/21.
//

#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <strings.h>

//channel还没添加到poller中
const int kNew = -1;
//channel已经添加到poller中
const int kAdded = 1;
//channel从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop) : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize)
{
    if(epollfd_ < 0) {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller() {
    ::close(epollfd_);
}

TimeStamp EPollPoller::poll(int timeoutMs, Poller::ChannelList *activeChannels) {
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__ , channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    TimeStamp now(TimeStamp::now());

    if(numEvents > 0) {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if(numEvents == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    }else if(numEvents == 0) {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }else {
        if(saveErrno != EINTR) {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() error!\n");
        }
    }

    return now;
}

void EPollPoller::updateChannel(Channel *channel) {
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__ , channel->fd(), channel->events(), index);

    if(index == kNew || index == kDeleted) {
        if(index == kNew) {
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }else {
        int fd = channel->fd();
        if(channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel *channel) {
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);

    int index = channel->index();
    if(index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }

    channel->set_index(kNew);
}

void EPollPoller::fillActiveChannels(int numEvents, Poller::ChannelList *activeChannels) const {
    for(int i = 0;i < numEvents; i++) {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

void EPollPoller::update(int operation, Channel *channel) {
    epoll_event event;
    bzero(&event, sizeof(event));

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;

    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if(operation == EPOLL_CTL_DEL) {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }else {
            LOG_FATAL("epoll_crl add/mod error:%d\n", errno);
        }
    }
}
```

## 获取线程tid代码

没什么好解释的，实现的一个静态方法来快速的获取当前的线程tid。

__thread表明变量是一个线程特有的局部变量。

### CurrentThread.h

```c++
//
// Created by jianp on 2025/12/7.
//

#ifndef MUDUOSELF_CURRENTTHREAD_H
#define MUDUOSELF_CURRENTTHREAD_H

#include <unistd.h>

namespace CurrentThread {
    extern __thread int t_cachedTid;

    void cacheTid();

    inline int tid() {
        if(__builtin_expect(t_cachedTid == 0, 0)) {//内建函数用来加快判断速度
            cacheTid();
        }
        return t_cachedTid;
    }//此处的cacheTid是小概率事件，return动作是大概率事件
};


#endif //MUDUOSELF_CURRENTTHREAD_H

```

### CurrentThread.cpp

```c++
//
// Created by jianp on 2025/12/7.
//

#include "CurrentThread.h"
#include <sys/syscall.h>

namespace CurrentThread {
    __thread int t_cachedTid = 0;

    void cacheTid() {
        if(t_cachedTid == 0) {
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}
```

这里通过命名空间来实现对当前线程tid的获取，使用了系统调用函数。

## EventLoop事件循环

网络库中对发生事件进行处理的主要部分。

### EventLoop.h

```c++
//
// Created by jianp on 2025/12/7.
//

#ifndef MUDUOSELF_EVENTLOOP_H
#define MUDUOSELF_EVENTLOOP_H

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "TimeStamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;

class EventLoop : noncopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    TimeStamp pollReturnTime() const { return pollReturnTime_; }

    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);

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
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_;
    std::vector<Functor> pendingFunctors_;
    std::mutex mutex_;
};


#endif //MUDUOSELF_EVENTLOOP_H

```

### EventLoop.cpp

```c++
//
// Created by jianp on 2025/12/7.
//

#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

__thread EventLoop *t_loopInThisThread = nullptr;

const int kPollTimeMs = 10000;

int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0) {
        LOG_FATAL("eventfd error:%d\n", errno);
    }

    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_)) {
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if(t_loopInThisThread) {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }else {
        t_loopInThisThread = this;
    }

    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while(!quit_) {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for(Channel* channel : activeChannels_) {
            channel->handleEvent(pollReturnTime_);
        }
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping. \n", this);
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
        queueInLoop(cb);
    }
}

void EventLoop::queueInLoop(EventLoop::Functor cb) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    if(!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)) {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8 \n", n);
    }
}

void EventLoop::wakeup() {
    u_int64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)) {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

void EventLoop::updateChannel(Channel *channel) {
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel) {
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel) {
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
```

## Thread线程类

### Thread.h

```c++
//
// Created by jianp on 2025/12/9.
//

#ifndef MUDUOSELF_THREAD_H
#define MUDUOSELF_THREAD_H

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

class Thread : noncopyable {
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string& name() const { return name_; }

    static int numCreated() { return numCreated_; }

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;
    static std::atomic_int numCreated_;
};


#endif //MUDUOSELF_THREAD_H

```

### Thread.cpp

```c++
//
// Created by jianp on 2025/12/9.
//

#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(Thread::ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name) {
    setDefaultName();
}

Thread::~Thread() {
    if(started_ && !joined_) {
        thread_->detach();
    }
}

void Thread::start() {
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        func_;
    }));

    sem_wait(&sem);
}

void Thread::join() {
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName() {
    int num = ++numCreated_;
    if(name_.empty()) {
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}
```

## EventLoopThread事件线程类讲解

其实就是将EventLoop放入一个Thread中去执行，也即one loop per thread的具体实现。

### EventLoopThread.h

```c++
//
// Created by jianp on 2025/12/9.
//

#ifndef MUDUOSELF_EVENTLOOPTHREAD_H
#define MUDUOSELF_EVENTLOOPTHREAD_H

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

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


#endif //MUDUOSELF_EVENTLOOPTHREAD_H

```

### EventLoopThread.cpp

```c++
//
// Created by jianp on 2025/12/9.
//

#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const EventLoopThread::ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this))
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
    thread_.start();

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop == nullptr) {
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
```

## EventLoopThreadPool池

### EventLoopThreadPool.h

```c++
//
// Created by jianp on 2025/12/10.
//

#ifndef MUDUOSELF_EVENTLOOPTHREADPOOL_H
#define MUDUOSELF_EVENTLOOPTHREADPOOL_H

#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }
    const std::string name() const { return name_; }
private:
    EventLoop* baseLoop_;
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};


#endif //MUDUOSELF_EVENTLOOPTHREADPOOL_H

```

### EventLoopThreadPool.cpp

```c++
//
// Created by jianp on 2025/12/10.
//

#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"

#include <memory>

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0) {

}

EventLoopThreadPool::~EventLoopThreadPool() {

}

void EventLoopThreadPool::start(const EventLoopThreadPool::ThreadInitCallback &cb) {
    started_ = true;

    for(int i = 0;i < numThreads_; i++) {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        EventLoopThread* t = new EventLoopThread(cb, buf);
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
```

## Socket封装fd

### Socket.h

```c++
//
// Created by jianp on 2025/12/10.
//

#ifndef MUDUOSELF_SOCKET_H
#define MUDUOSELF_SOCKET_H

#include "noncopyable.h"

class InetAddress;

class Socket : noncopyable {
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    ~Socket();

    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress& localaddr);
    void listen();
    int accept(InetAddress* peeraddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);
private:
    const int sockfd_;
};


#endif //MUDUOSELF_SOCKET_H

```

### Socket.cpp

```c++
//
// Created by jianp on 2025/12/10.
//

#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

Socket::~Socket() {
    close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr) {
    if(::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)) != 0) {
        LOG_FATAL("bind socket:%d fail \n", sockfd_);
    }
}

void Socket::listen() {
    if(::listen(sockfd_, 1024) != 0) {
        LOG_FATAL("listen sockfd:%d fail \n", sockfd_);
    }
}

int Socket::accept(InetAddress *peeraddr) {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    bzero(&addr, sizeof(addr));
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(connfd >= 0) {
        peeraddr->setSockAddr(addr);
    }

    return connfd;
}

void Socket::shutdownWrite() {
    if(::shutdown(sockfd_, SHUT_WR) < 0) {
        LOG_ERROR("shutdownWrite error\n");
    }
}

void Socket::setTcpNoDelay(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}
```

## Acceptor代码详解

### Acceptor.h

```c++
//
// Created by jianp on 2025/12/10.
//

#ifndef MUDUOSELF_ACCEPTOR_H
#define MUDUOSELF_ACCEPTOR_H

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : noncopyable {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb) {
        newConnectionCallback_ = cb;
    }

    bool listenning() const { return listenning_; }
    void listen();
private:
    void handleRead();

    EventLoop *loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;

    bool listenning_;
};


#endif //MUDUOSELF_ACCEPTOR_H

```

### Acceptor.cpp

```c++
//
// Created by jianp on 2025/12/10.
//

#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

static int createNonblocking() {
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(sockfd < 0) {
        LOG_FATAL("%s:%s:%d listen socket create error:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false) {
    acceptSocket_.setReusePort(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen() {
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead() {
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0) {
        if(newConnectionCallback_) {
            newConnectionCallback_(connfd, peerAddr);
        }else {
            ::close(connfd);
        }
    }else {
        LOG_ERROR("%s:%s:%d accept error:%d \n", __FILE__, __FUNCTION__ , __LINE__, errno);
        if(errno == EMFILE) {
            LOG_ERROR("%s:%s:%d sockfd reached limit! \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }

}

```

## TcpServer代码讲解

### TcpServer.h

```c++
//
// Created by jianp on 2025/12/11.
//

#ifndef MUDUOSELF_TCPSERVER_H
#define MUDUOSELF_TCPSERVER_H

#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

class TcpServer : noncopyable {
public:
    using ThreadinitCallback = std::function<void(EventLoop*)>;

    enum Option {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option = kNoReusePort);
    ~TcpServer();


    void setThreadInitCallback(const ThreadinitCallback& cb) { threadinitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

    void setThreadNum(int numThreads);

    void start();
private:
    void newConnection(int sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    EventLoop* loop_;

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;
    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;

    ThreadinitCallback threadinitCallback_;

    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_;
};


#endif //MUDUOSELF_TCPSERVER_H

```

### TcpServer.cpp

```c++
//
// Created by jianp on 2025/12/11.
//

#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <strings.h>
#include <functional>

static EventLoop* CheckLoopNotNull(EventLoop* loop) {
    if(loop == nullptr) {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }

    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg,
                     TcpServer::Option option)
                     : loop_(CheckLoopNotNull(loop))
                     , ipPort_(listenAddr.toIpPort())
                     , name_(nameArg)
                     , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
                     , threadPool_(new EventLoopThreadPool(loop, name_))
                     , connectionCallback_()
                     , messageCallback_()
                     , nextConnId_(1)
                     , started_(0) {
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                                  std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer() {
    for(auto& item : connections_) {
        TcpConnectionPtr conn(item.second);
        item.second.reset();

        conn->getLoop()->runInLoop(
                std::bind(&TcpConnection::connectDestroyed, conn)
                );
    }
}

void TcpServer::setThreadNum(int numThreads) {
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start() {
    if(started_++ == 0) {
        threadPool_->start(threadinitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    sockaddr_in local;
    ::bzero(&local, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if(::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0) {
        LOG_ERROR("sockets::getLocalAddr\n");
    }
    InetAddress localAddr(local);

    TcpConnectionPtr conn(new TcpConnection(
            ioLoop,
            connName,
            sockfd,
            localAddr,
            peerAddr));
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    conn->setCloseCallback(
            std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
            );

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    loop_->runInLoop(
            std::bind(&TcpServer::removeConnectionInLoop, this, conn)
            );
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
             name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn)
            );
}
```

## Buffer缓冲区

### Buffer.h

```c++
//
// Created by jianp on 2025/12/10.
//

#ifndef MUDUOSELF_BUFFER_H
#define MUDUOSELF_BUFFER_H

#include <vector>
#include <string>
#include <algorithm>

class Buffer {
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend) {

    }

    size_t readableBytes() const {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const {
        return readerIndex_;
    }

    const char* peek() const {//返回buffer中可读数据的起始地址
        return begin() + readerIndex_;
    }

    void retrieve(size_t len) {
        if(len < readableBytes()) {
            readerIndex_ += len;
        }else {
            retrieveAll();
        }
    }

    void retrieveAll() {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    std::string retrieveAllAsString() {
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len) {
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    void ensureWritableBytes(size_t len) {
        if(writableBytes() < len) {
            makeSpace(len);
        }
    }

    void append(const char* data, size_t len) {
        makeSpace(len);
        std::copy(data, data + len, beginWirte());
        writerIndex_ += len;
    }

    char* beginWirte() {
        return begin() + writerIndex_;
    }

    const char* beginWirte() const {
        return begin() + writerIndex_;
    }

    ssize_t readFd(int fd, int* saveErrno);
    ssize_t writeFd(int fd, int* saveErrno);
private:
    char* begin() {
        return &*buffer_.begin();
    }

    const char* begin() const {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len) {
        if(writableBytes() + prependableBytes() < len + kCheapPrepend) {
            buffer_.resize(writerIndex_ + len);
        }else {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};


#endif //MUDUOSELF_BUFFER_H

```

### Buffer.cpp

```c++
//
// Created by jianp on 2025/12/10.
//

#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

ssize_t Buffer::readFd(int fd, int *saveErrno) {
    char extrabuf[65536] = {0};

    struct iovec vec[2];

    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if(n < 0) {
        *saveErrno = errno;
    }else if(n <= writable) {
        writerIndex_ += n;
    }else {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno) {
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0) {
        *saveErrno = errno;
    }

    return n;
}
```

## TcpConnection连接

### TcpConnection.h

```c++
//
// Created by jianp on 2025/12/11.
//

#ifndef MUDUOSELF_TCPCONNECTION_H
#define MUDUOSELF_TCPCONNECTION_H

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "TimeStamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(EventLoop* loop,
                  const std::string& nameArg,
                  int sockfd,
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    void send(const std::string& buf);
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
        writeCompleteCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb) {
        highWaterMarkCallback_ = cb;
    }

    void setCloseCallback(const CloseCallback& cb) {
        closeCallback_ = cb;
    }

    void connectEstablished();
    void connectDestroyed();
private:
    enum StateE {
        kDisconnected, kConnecting, kConnected, kDisconnecting
    };
    void setState(StateE state) { state_ = state; }

    void handleRead(TimeStamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();

    EventLoop* loop_;
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;
};


#endif //MUDUOSELF_TCPCONNECTION_H

```

### TcpConnection.cpp

```c++
//
// Created by jianp on 2025/12/11.
//

#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <string>

static EventLoop* CheckLoopNotNull(EventLoop* loop) {
    if(loop == nullptr) {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }

    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg, int sockfd, const InetAddress &localAddr,
                             const InetAddress &peerAddr)
                             : loop_(CheckLoopNotNull(loop))
                             , name_(nameArg)
                             , state_(kConnecting)
                             , reading_(true)
                             , socket_(new Socket(sockfd))
                             , channel_(new Channel(loop, sockfd))
                             , localAddr_(localAddr)
                             , peerAddr_(peerAddr)
                             , highWaterMark_(64 * 1024 * 1024) {
    channel_->setReadCallback(
            std::bind(&TcpConnection::handleRead, this, std::placeholders::_1)
            );
    channel_->setWriteCallback(
            std::bind(&TcpConnection::handleWrite, this)
            );
    channel_->setCloseCallback(
            std::bind(&TcpConnection::handleClose, this)
            );
    channel_->setErrorCallback(
            std::bind(&TcpConnection::handleError, this)
            );

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n",
             name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string &buf) {
    if(state_ == kConnected) {
        if(loop_->isInLoopThread()) {
            sendInLoop(buf.c_str(), buf.size());
        }else {
            loop_->runInLoop(
                    std::bind(
                            &TcpConnection::sendInLoop,
                            this,
                            buf.c_str(),
                            buf.size()
                            )
                    );
        }
    }
}

void TcpConnection::sendInLoop(const void *message, size_t len) {
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if(state_ == kDisconnecting) {
        LOG_ERROR("disconnected, give up writing!");
        return ;
    }

    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), message, len);
        if(nwrote >= 0) {
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_) {
                loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())
                        );
            }
        }else {
            nwrote = 0;
            if(errno != EWOULDBLOCK) {
                LOG_ERROR("TcpConnection::sendInLoop");
                if(errno == EPIPE || errno == ECONNRESET) {
                     faultError = true;
                }
            }
        }
    }

    if(!faultError && remaining > 0) {
        size_t oldLen = outputBuffer_.readableBytes();
        if(oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_) {
            loop_->queueInLoop(
                    std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining)
                    );
        }

        outputBuffer_.append((char*)message + nwrote, remaining);
        if(!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdown() {
    if(state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(
                std::bind(&TcpConnection::shutdownInLoop, this)
                );
    }
}

void TcpConnection::shutdownInLoop() {
    if(!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}

void TcpConnection::connectEstablished() {
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed() {
    if(state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }

    channel_->remove();
}

void TcpConnection::handleRead(TimeStamp receiveTime) {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if(n > 0) {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }else if(n == 0) {
        handleClose();
    }else {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead\n");
        handleError();
    }
}

void TcpConnection::handleWrite() {
    if(channel_->isWriting()) {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if(n > 0) {
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting();
                if(writeCompleteCallback_) {
                    loop_->queueInLoop(
                            std::bind(writeCompleteCallback_, shared_from_this())
                            );
                }
                if(state_ == kDisconnecting) {
                    shutdownInLoop();
                }
            }
        }else {
            LOG_ERROR("TcpConnection::handleWirte\n");
        }
    }else {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
}

void TcpConnection::handleClose() {
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);
    closeCallback_(connPtr);
}

void TcpConnection::handleError() {
    int optVal;
    socklen_t optLen = sizeof(optVal);
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optVal, &optLen) < 0) {
        err = errno;
    }else {
        err = optVal;
    }

    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}



```

## TcpServer终章

## 编译安装脚本以及项目测试代码

