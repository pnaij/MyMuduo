//
// Created by jianp on 2025/12/11.
//

#include "jpmuduo/net/Poller.h"
#include "jpmuduo/net/poller/EPollPoller.h"
#include "jpmuduo/net/poller/PollPoller.h"

#include <stdlib.h>

namespace jpmuduo {

Poller* Poller::newDefaultPoller(EventLoop *loop) {
    if(::getenv("MUDUO_USE_POLL")) {
        return new PollPoller(loop);
    }else {
        return new EPollPoller(loop);
    }
}

}  // namespace jpmuduo
