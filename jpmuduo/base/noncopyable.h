//
// Created by jianp on 2025/11/7.
//

#ifndef JPMUDUO_NONCOPYABLE_H
#define JPMUDUO_NONCOPYABLE_H

namespace jpmuduo
{

class noncopyable {
public:
    noncopyable(const noncopyable&) = delete;
    void operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};

}  // namespace jpmuduo

#endif //JPMUDUO_NONCOPYABLE_H
