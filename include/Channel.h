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
    void set_revents(int revt) { revents_ = revt; }

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

    EventLoop *loop_;   //表示当前channel由那个eventloop负责
    const int fd_;
    int events_;        //表示当前fd感兴趣的事件
    int revents_;       //表示当前fd上实际发生的事件
    int index_;         //index用来标记当前channel在poller中的状态

    std::weak_ptr<void> tie_;       //channel常与tcpconnection对象绑定，tie用来保证在channel进行事件处理时对应的connection对象没有被析构
    bool tied_;

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

    void update();        //更新channel状态
    void handleEventWithGuard(TimeStamp receiveTime);
};


#endif //MUDUOSELF_CHANNEL_H
