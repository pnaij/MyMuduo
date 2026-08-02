//
// Created by jianp on 2026/5/16.
//

#include "jpmuduo/net/poller/PollPoller.h"
#include "jpmuduo/net/Channel.h"
#include "jpmuduo/base/Logger.h"

#include <errno.h>
#include <unistd.h>

namespace jpmuduo {

// channel还没添加到poller中
const int kNew = -1;
// channel已经添加到poller中
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;

PollPoller::PollPoller(EventLoop *loop) : Poller(loop) {}

PollPoller::~PollPoller() = default;

TimeStamp PollPoller::poll(int timeoutMs, Poller::ChannelList *activeChannels) {
    pollfds_.clear();
    for (const auto &pair : channels_) {
        struct pollfd pfd = {};
        pfd.fd = pair.second->fd();
        pfd.events = pair.second->events();
        pollfds_.push_back(pfd);
    }

    LOG_DEBUG("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

    int numEvents = ::poll(pollfds_.data(), pollfds_.size(), timeoutMs);
    int saveErrno = errno;
    TimeStamp now(TimeStamp::now());

    if (numEvents > 0) {
        LOG_DEBUG("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
    } else if (numEvents == 0) {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    } else {
        if (saveErrno != EINTR) {
            errno = saveErrno;
            LOG_ERROR("PollPoller::poll() error!\n");
        }
    }

    return now;
}

void PollPoller::updateChannel(Channel *channel) {
    const int index = channel->index();
    LOG_DEBUG("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted) {
        if (index == kNew) {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
    }
}

void PollPoller::removeChannel(Channel *channel) {
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_DEBUG("func=%s => fd=%d \n", __FUNCTION__, fd);

    int index = channel->index();
    if (index == kAdded) {
        // pollfds is rebuilt on each poll(), no explicit removal needed
    }
    channel->set_index(kNew);
}

void PollPoller::fillActiveChannels(int numEvents, Poller::ChannelList *activeChannels) const {
    for (int i = 0; i < static_cast<int>(pollfds_.size()) && numEvents > 0; i++) {
        if (pollfds_[i].revents != 0) {
            auto it = channels_.find(pollfds_[i].fd);
            if (it != channels_.end()) {
                Channel *channel = it->second;
                channel->set_revents(pollfds_[i].revents);
                activeChannels->push_back(channel);
            }
            numEvents--;
        }
    }
}

}  // namespace jpmuduo
