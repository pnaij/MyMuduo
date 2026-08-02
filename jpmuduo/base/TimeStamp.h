//
// Created by jianp on 2025/11/8.
//

#ifndef JPMUDUO_TIMESTAMP_H
#define JPMUDUO_TIMESTAMP_H

#include <string>
#include <cstdint>

namespace jpmuduo
{

class TimeStamp {
public:
    static constexpr int kMicroSecondsPerSecond = 1000 * 1000;

    TimeStamp();
    explicit TimeStamp(int64_t microSecondsSinceEpoch);

    static TimeStamp now();
    static TimeStamp addTime(const TimeStamp& ts, double seconds);

    // Machine-parseable pure numeric: "sec.usec" e.g. "8245137.123456"
    std::string toString() const;

    // Human-readable wall-clock: "2026-05-19 15:30:00.123456"
    std::string toFormattedString() const;

    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }
    bool valid() const { return microSecondsSinceEpoch_ > 0; }

private:
    int64_t microSecondsSinceEpoch_;
};

inline bool operator<(const TimeStamp& lhs, const TimeStamp& rhs) {
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline bool operator<=(const TimeStamp& lhs, const TimeStamp& rhs) {
    return lhs.microSecondsSinceEpoch() <= rhs.microSecondsSinceEpoch();
}

inline bool operator==(const TimeStamp& lhs, const TimeStamp& rhs) {
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

inline bool operator!=(const TimeStamp& lhs, const TimeStamp& rhs) {
    return lhs.microSecondsSinceEpoch() != rhs.microSecondsSinceEpoch();
}

}  // namespace jpmuduo

#endif //JPMUDUO_TIMESTAMP_H
