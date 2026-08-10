// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef JPMUDUO_THREADPOOL_H
#define JPMUDUO_THREADPOOL_H

#include "jpmuduo/base/Condition.h"
#include "jpmuduo/base/Mutex.h"
#include "jpmuduo/base/Thread.h"

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace jpmuduo
{

class ThreadPool : noncopyable
{
 public:
  typedef std::function<void ()> Task;

  explicit ThreadPool(const std::string& nameArg = std::string("ThreadPool"));
  ~ThreadPool();

  void setMaxQueueSize(int maxSize) { maxQueueSize_ = maxSize; }
  void setThreadInitCallback(const Task& cb)
  { threadInitCallback_ = cb; }

  void start(int numThreads);
  void stop();

  const std::string& name() const
  { return name_; }

  size_t queueSize() const;

  void run(Task f);

 private:
  bool isFull() const REQUIRES(mutex_);
  void runInThread();
  Task take();

  mutable MutexLock mutex_;
  Condition notEmpty_ GUARDED_BY(mutex_);
  Condition notFull_ GUARDED_BY(mutex_);
  std::string name_;
  Task threadInitCallback_;
  std::vector<std::unique_ptr<Thread>> threads_;
  std::deque<Task> queue_ GUARDED_BY(mutex_);
  size_t maxQueueSize_;
  bool running_;
};

}  // namespace jpmuduo

#endif  // JPMUDUO_THREADPOOL_H