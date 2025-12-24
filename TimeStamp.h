//
// Created by jianp on 2025/11/8.
//

#ifndef MUDUOSELF_TIMESTAMP_H
#define MUDUOSELF_TIMESTAMP_H

#include <iostream>
#include <string>
#include <cstdint>

class TimeStamp {
public:
    TimeStamp();
    explicit TimeStamp(int64_t microSecondsSinceEpoch);
    static TimeStamp now();
    std::string toString() const;

private:
    int64_t microSecondsSinceEpoch_;
};


#endif //MUDUOSELF_TIMESTAMP_H
