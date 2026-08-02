//
// Created by jianp on 2025/12/7.
//

#include "jpmuduo/base/CurrentThread.h"

namespace jpmuduo
{
namespace CurrentThread {
    __thread int t_cachedTid = 0;

    void cacheTid() {
        if(t_cachedTid == 0) {
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}
}  // namespace jpmuduo
