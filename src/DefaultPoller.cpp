//
// Created by jianp on 2025/12/11.
//

#include "../include/Poller.h"
#include "../include/EPollPoller.h"

#include <stdlib.h>

Poller* Poller::newDefaultPoller(EventLoop *loop) {
    if(::getenv("MUDUO_USE_POLL")) {
        return nullptr;
    }else {
        return new EPollPoller(loop);
    }
}
