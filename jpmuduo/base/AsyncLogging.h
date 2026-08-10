// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef JPMUDUO_ASYNCLOGGING_H
#define JPMUDUO_ASYNCLOGGING_H

#include "jpmuduo/base/BlockingQueue.h"
#include "jpmuduo/base/BoundedBlockingQueue.h"
#include "jpmuduo/base/CountDownLatch.h"
#include "jpmuduo/base/Mutex.h"
#include "jpmuduo/base/Thread.h"
#include "jpmuduo/base/LogStream.h"

#include <atomic>
#include <string>
#include <vector>

namespace jpmuduo
{

class AsyncLogging : noncopyable
{
 public:

  AsyncLogging(const std::string& basename,
               off_t rollSize,
               int flushInterval = 3);

  ~AsyncLogging()
  {
    if (running_)
    {
      stop();
    }
  }

  void append(const char* logline, int len);

  void start()
  {
    running_ = true;
    thread_.start();
    latch_.wait();
  }

  void stop() NO_THREAD_SAFETY_ANALYSIS
  {
    running_ = false;
    cond_.notify();
    thread_.join();
  }

 private:

  void threadFunc();

  typedef jpmuduo::detail::FixedBuffer<jpmuduo::detail::kLargeBuffer> Buffer;
  typedef std::vector<std::unique_ptr<Buffer>> BufferVector;
  typedef BufferVector::value_type BufferPtr;

  const int flushInterval_;
  std::atomic<bool> running_;
  const std::string basename_;
  const off_t rollSize_;
  jpmuduo::Thread thread_;
  jpmuduo::CountDownLatch latch_;
  jpmuduo::MutexLock mutex_;
  jpmuduo::Condition cond_ GUARDED_BY(mutex_);
  BufferPtr currentBuffer_ GUARDED_BY(mutex_);
  BufferPtr nextBuffer_ GUARDED_BY(mutex_);
  BufferVector buffers_ GUARDED_BY(mutex_);
};

}  // namespace jpmuduo

#endif  // JPMUDUO_ASYNCLOGGING_H