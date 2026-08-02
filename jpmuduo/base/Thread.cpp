//
// Created by jianp on 2025/12/9.
//

#include "jpmuduo/base/Thread.h"
#include "jpmuduo/base/CurrentThread.h"

#include <semaphore.h>

namespace jpmuduo
{

std::atomic_int Thread::numCreated_(0);

Thread::Thread(Thread::ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name) {
    setDefaultName();
}

Thread::~Thread() {
    if(started_ && !joined_) {
        thread_->detach();
    }
}

void Thread::start() {
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    thread_ = std::make_shared<std::thread>([&]() {
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        func_();        //也就是执行loop
    });

    sem_wait(&sem);
}

void Thread::join() {
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName() {
    int num = ++numCreated_;
    if(name_.empty()) {
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}
}  // namespace jpmuduo