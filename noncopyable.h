//
// Created by jianp on 2025/11/7.
//

#ifndef MUDUOSELF_NONCOPYABLE_H
#define MUDUOSELF_NONCOPYABLE_H

class noncopyable {
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};

#endif //MUDUOSELF_NONCOPYABLE_H
