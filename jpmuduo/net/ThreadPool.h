//
// Created by jianp on 2026/5/19.
//

#ifndef JPMUDUO_THREADPOOL_H
#define JPMUDUO_THREADPOOL_H

#include "jpmuduo/base/noncopyable.h"

#include <functional>
#include <memory>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace jpmuduo {

class ThreadPool : noncopyable {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(const std::string& name = std::string());
    ~ThreadPool();

    void setMaxSize(int maxSize) { maxSize_ = maxSize; }

    void start(int numThreads);
    void stop();

    size_t queueSize() const;

    // Submit a task; returns immediately.
    // TcpConnection::send already detects the calling thread and posts to
    // the correct IO thread, so the worker can call conn->send directly:
    //
    //   pool->run([conn, data]() {
    //       auto result = heavyCompute(data);
    //       conn->send(result);  // ThreadPool thread → auto queued to IO loop
    //   });
    void run(Task task);

private:
    void runInThread();
    Task take();

    std::string name_;
    std::vector<std::thread> threads_;
    std::deque<Task> queue_;
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    int maxSize_;
    bool running_;
};

}  // namespace jpmuduo

#endif // JPMUDUO_THREADPOOL_H
