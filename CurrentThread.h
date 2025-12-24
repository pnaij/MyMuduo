//
// Created by jianp on 2025/12/7.
//

#ifndef MUDUOSELF_CURRENTTHREAD_H
#define MUDUOSELF_CURRENTTHREAD_H

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread {
    extern __thread int t_cachedTid;

    void cacheTid();

    inline int tid() {
        if(__builtin_expect(t_cachedTid == 0, 0)) {
            cacheTid();
        }
        return t_cachedTid;
    }
};


#endif //MUDUOSELF_CURRENTTHREAD_H
