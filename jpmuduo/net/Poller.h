//
// Created by jianp on 2025/11/21.
//

#ifndef JPMUDUO_POLLER_H
#define JPMUDUO_POLLER_H

#include "jpmuduo/base/noncopyable.h"
#include "jpmuduo/base/TimeStamp.h"
#include "jpmuduo/net/EventLoop.h"

#include <vector>
#include <unordered_map>

namespace jpmuduo {

class Channel;
class EventLoop;

class Poller : noncopyable {
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller() = default;

    virtual TimeStamp poll(int timeOutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    bool hasChannel(Channel *channel) const;

    static Poller* newDefaultPoller(EventLoop *loop);

    void assertInLoopThread() const
    {
        ownerLoop_->assertInLoopThread();
    }

protected:
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;
private:
    EventLoop *ownerLoop_;
};


}  // namespace jpmuduo

#endif //JPMUDUO_POLLER_H
