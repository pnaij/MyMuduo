// CountDownLatch 单元测试（原始 muduo clone 中没有此模板，自编）
//
// 验证点：
//  1. 两个线程各 countDown 一次，主线程 wait() 返回后 getCount() == 0
//  2. wait() 不会提前返回：另一个线程延迟 300ms 才 countDown，
//     主线程 wait() 的阻塞时间应不小于 ~300ms
//
// jpmuduo 适配：CurrentThread 无 name()，打印用 tid()；
// 计时用 TimeStamp::now()（CLOCK_MONOTONIC）差值，单位毫秒。
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "jpmuduo/base/CountDownLatch.h"
#include "jpmuduo/base/CurrentThread.h"
#include "jpmuduo/base/Thread.h"
#include "jpmuduo/base/TimeStamp.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

using jpmuduo::CountDownLatch;
// 注意：CurrentThread 是 namespace 而非类，直接用全限定名 jpmuduo::CurrentThread::tid()
using jpmuduo::Thread;
using jpmuduo::TimeStamp;

struct Helper
{
  CountDownLatch* latch;

  void run1() const
  {
    printf("tid=%d, countDown 1\n", jpmuduo::CurrentThread::tid());
    latch->countDown();
  }

  void run2() const
  {
    printf("tid=%d, countDown 2\n", jpmuduo::CurrentThread::tid());
    latch->countDown();
  }

  void runAfterDelay() const
  {
    // 延迟 300ms 再 countDown
    usleep(300*1000);
    printf("tid=%d, countDown after delay\n", jpmuduo::CurrentThread::tid());
    latch->countDown();
  }
};

// 验证 1：两个线程各 countDown 一次，主线程 wait() 返回后 count == 0
void testCountDownToZero()
{
  CountDownLatch latch(2);
  Helper helper = { &latch };

  Thread t1([&helper] { helper.run1(); }, "cdl-1");
  Thread t2([&helper] { helper.run2(); }, "cdl-2");
  t1.start();
  t2.start();

  printf("main: waiting for count down\n");
  latch.wait();
  printf("main: wait() returned, count = %d\n", latch.getCount());
  assert(latch.getCount() == 0);

  t1.join();
  t2.join();
}

// 验证 2：wait() 不会提前返回 —— 延迟 countDown + 计时
void testWaitBlocks()
{
  CountDownLatch latch(1);
  Helper helper = { &latch };

  Thread t1([&helper] { helper.runAfterDelay(); }, "cdl-delay");
  t1.start();

  TimeStamp t0 = TimeStamp::now();
  latch.wait();
  TimeStamp t1_ = TimeStamp::now();
  double elapsedMs = (t1_.microSecondsSinceEpoch() - t0.microSecondsSinceEpoch()) / 1000.0;
  printf("main: wait() returned after %.1f ms, count = %d\n",
         elapsedMs, latch.getCount());
  // 延迟 300ms，wait() 阻塞时间应 >= 250ms（留 50ms 容差）
  assert(elapsedMs >= 250.0);
  assert(latch.getCount() == 0);

  t1.join();
}

int main()
{
  printf("pid=%d, tid=%d\n", ::getpid(), jpmuduo::CurrentThread::tid());

  testCountDownToZero();
  testWaitBlocks();

  printf("All passed.\n");
  return 0;
}
