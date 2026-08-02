//
// Created by jianp on 2026/5/16.
//

#ifndef JPMUDUO_POLLPOLLER_H
#define JPMUDUO_POLLPOLLER_H

#include "jpmuduo/net/Poller.h"

#include <vector>
#include <poll.h>

namespace jpmuduo {

class PollPoller : public Poller {
public:
    explicit PollPoller(EventLoop *loop);
    ~PollPoller() override;

    TimeStamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    using PollFdList = std::vector<struct pollfd>;

    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

    PollFdList pollfds_;
};

}  // namespace jpmuduo

#endif // JPMUDUO_POLLPOLLER_H
