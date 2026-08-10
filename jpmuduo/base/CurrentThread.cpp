//
// Created by jianp on 2025/12/7.
//

#include "jpmuduo/base/CurrentThread.h"

#include <stdio.h>

namespace jpmuduo
{
namespace CurrentThread {
    __thread int t_cachedTid = 0;
    __thread char t_tidString[32];
    __thread int t_tidStringLength = 6;

    void cacheTid() {
        if(t_cachedTid == 0) {
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
            t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
        }
    }
}
}  // namespace jpmuduo