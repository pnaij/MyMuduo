//
// Created by jianp on 2026/5/19.
//

#include "jpmuduo/net/ThreadPool.h"

#include <assert.h>

namespace jpmuduo {

ThreadPool::ThreadPool(const std::string& name)
    : name_(name)
    , maxSize_(-1)
    , running_(false) {
}

ThreadPool::~ThreadPool() {
    if (running_) {
        stop();
    }
}

void ThreadPool::start(int numThreads) {
    assert(threads_.empty());
    running_ = true;
    threads_.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i) {
        threads_.emplace_back([this]() { runInThread(); });
    }
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        notEmpty_.notify_all();
        notFull_.notify_all();
    }
    for (auto& t : threads_) {
        t.join();
    }
    threads_.clear();
}

size_t ThreadPool::queueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

void ThreadPool::run(Task task) {
    if (threads_.empty()) {
        task();
    } else {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            while (maxSize_ > 0 && static_cast<int>(queue_.size()) >= maxSize_) {
                notFull_.wait(lock);
            }
            queue_.push_back(std::move(task));
        }
        notEmpty_.notify_one();
    }
}

ThreadPool::Task ThreadPool::take() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (queue_.empty() && running_) {
        notEmpty_.wait(lock);
    }
    Task task;
    if (!queue_.empty()) {
        task = queue_.front();
        queue_.pop_front();
        notFull_.notify_one();
    }
    return task;
}

void ThreadPool::runInThread() {
    while (running_) {
        Task task = take();
        if (task) {
            task();
        }
    }
}

}  // namespace jpmuduo
