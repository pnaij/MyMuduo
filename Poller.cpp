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
