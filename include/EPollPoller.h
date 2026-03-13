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

class EPollPoller : public Poller {
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
