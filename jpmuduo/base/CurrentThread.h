//
// Created by jianp on 2025/12/7.
//

#ifndef JPMUDUO_CURRENTTHREAD_H
#define JPMUDUO_CURRENTTHREAD_H

#include <string>
#include <unistd.h>
#include <sys/syscall.h>

namespace jpmuduo
{
namespace CurrentThread {
    extern __thread int t_cachedTid;
    extern __thread char t_tidString[32];
    extern __thread int t_tidStringLength;

    void cacheTid();

    inline int tid() {
        if(__builtin_expect(t_cachedTid == 0, 0)) {
            cacheTid();
        }
        return t_cachedTid;
    }

    inline const char* tidString() // for logging
    {
        return t_tidString;
    }

    inline int tidStringLength() // for logging
    {
        return t_tidStringLength;
    }

    std::string stackTrace(bool demangle);
};
}  // namespace jpmuduo

#endif //JPMUDUO_CURRENTTHREAD_H