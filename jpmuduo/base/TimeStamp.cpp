//
// Created by jianp on 2025/11/8.
//

#include "jpmuduo/base/TimeStamp.h"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <cstdint>

namespace jpmuduo
{

TimeStamp::TimeStamp() : microSecondsSinceEpoch_(0) {}

TimeStamp::TimeStamp(int64_t microSecondsSinceEpoch)
    : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

TimeStamp TimeStamp::now() {
    struct timespec ts;
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return TimeStamp(static_cast<int64_t>(ts.tv_sec) * kMicroSecondsPerSecond
                     + ts.tv_nsec / 1000);
}

TimeStamp TimeStamp::addTime(const TimeStamp& ts, double seconds) {
    int64_t delta = static_cast<int64_t>(seconds * kMicroSecondsPerSecond);
    return TimeStamp(ts.microSecondsSinceEpoch() + delta);
}

int64_t TimeStamp::realtimeOffsetUs() {
    static const int64_t kOffset = [] {
        struct timespec mono, real;
        ::clock_gettime(CLOCK_MONOTONIC, &mono);
        ::clock_gettime(CLOCK_REALTIME, &real);
        int64_t monotonic = static_cast<int64_t>(mono.tv_sec) * kMicroSecondsPerSecond
                            + mono.tv_nsec / 1000;
        int64_t realtime = static_cast<int64_t>(real.tv_sec) * kMicroSecondsPerSecond
                           + real.tv_nsec / 1000;
        return realtime - monotonic;
    }();
    return kOffset;
}

std::string TimeStamp::toString() const {
    int64_t sec = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
    int64_t usec = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
    char buf[64];
    snprintf(buf, sizeof(buf), "%ld.%06ld", sec, usec);
    return buf;
}

std::string TimeStamp::toFormattedString() const {
    int64_t realUs = microSecondsSinceEpoch_ + realtimeOffsetUs();

    time_t sec = static_cast<time_t>(realUs / kMicroSecondsPerSecond);
    int64_t usec = realUs % kMicroSecondsPerSecond;

    struct tm tm_time;
    ::memset(&tm_time, 0, sizeof(tm_time));
    ::localtime_r(&sec, &tm_time);

    char buf[64];
    snprintf(buf, sizeof(buf), "%4d-%02d-%02d %02d:%02d:%02d.%06ld",
             tm_time.tm_year + 1900,
             tm_time.tm_mon + 1,
             tm_time.tm_mday,
             tm_time.tm_hour,
             tm_time.tm_min,
             tm_time.tm_sec,
             usec);
    return buf;
}
}  // namespace jpmuduo
