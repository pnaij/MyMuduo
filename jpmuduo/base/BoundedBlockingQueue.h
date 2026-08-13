// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef JPMUDUO_BOUNDEDBLOCKINGQUEUE_H
#define JPMUDUO_BOUNDEDBLOCKINGQUEUE_H

#include "jpmuduo/base/Condition.h"
#include "jpmuduo/base/Mutex.h"

#include <deque>
#include <assert.h>

namespace jpmuduo
{

template<typename T>
class BoundedBlockingQueue : noncopyable
{
 public:
  explicit BoundedBlockingQueue(int maxSize)
    : mutex_(),
      notEmpty_(mutex_),
      notFull_(mutex_),
      maxSize_(maxSize)
  {
  }

  void put(const T& x)
  {
    MutexLockGuard lock(mutex_);
    // 注意：不能调用 full()（会再次加锁自死锁），持锁时直接检查队列
    while (queue_.size() >= static_cast<size_t>(maxSize_))
    {
      notFull_.wait();
    }
    assert(queue_.size() < static_cast<size_t>(maxSize_));
    queue_.push_back(x);
    notEmpty_.notify();
  }

  void put(T&& x)
  {
    MutexLockGuard lock(mutex_);
    while (queue_.size() >= static_cast<size_t>(maxSize_))
    {
      notFull_.wait();
    }
    assert(queue_.size() < static_cast<size_t>(maxSize_));
    queue_.push_back(std::move(x));
    notEmpty_.notify();
  }

  T take()
  {
    MutexLockGuard lock(mutex_);
    while (queue_.empty())
    {
      notEmpty_.wait();
    }
    assert(!queue_.empty());
    T front(std::move(queue_.front()));
    queue_.pop_front();
    notFull_.notify();
    return front;
  }

  bool empty() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.empty();
  }

  bool full() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.size() >= static_cast<size_t>(maxSize_);
  }

  size_t size() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

  size_t capacity() const
  {
    MutexLockGuard lock(mutex_);
    return static_cast<size_t>(maxSize_);
  }

 private:
  mutable MutexLock mutex_;
  Condition         notEmpty_ GUARDED_BY(mutex_);
  Condition         notFull_ GUARDED_BY(mutex_);
  std::deque<T>     queue_ GUARDED_BY(mutex_);
  int               maxSize_;
};

}  // namespace jpmuduo

#endif  // JPMUDUO_BOUNDEDBLOCKINGQUEUE_H