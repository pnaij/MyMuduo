// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef JPMUDUO_COUNTDOWNLATCH_H
#define JPMUDUO_COUNTDOWNLATCH_H

#include "jpmuduo/base/Condition.h"
#include "jpmuduo/base/Mutex.h"

namespace jpmuduo
{

class CountDownLatch : noncopyable
{
 public:

  explicit CountDownLatch(int count);

  void wait();

  void countDown();

  int getCount() const;

 private:
  mutable MutexLock mutex_;
  Condition condition_ GUARDED_BY(mutex_);
  int count_ GUARDED_BY(mutex_);
};

}  // namespace jpmuduo

#endif  // JPMUDUO_COUNTDOWNLATCH_H