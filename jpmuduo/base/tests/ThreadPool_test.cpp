// 对照原始 muduo ThreadPool_test.cc 改写：
//   - namespace muduo -> jpmuduo
//   - jpmuduo 的 CurrentThread 没有 sleepUsec()，改用 usleep()
#include "jpmuduo/base/ThreadPool.h"
#include "jpmuduo/base/CountDownLatch.h"
#include "jpmuduo/base/CurrentThread.h"
#include "jpmuduo/base/Logging.h"

#include <stdio.h>
#include <unistd.h>  // usleep

void print()
{
  printf("tid=%d\n", jpmuduo::CurrentThread::tid());
}

void printString(const std::string& str)
{
  LOG_INFO << str;
  usleep(100*1000);
}

void test(int maxSize)
{
  LOG_WARN << "Test ThreadPool with max queue size = " << maxSize;
  jpmuduo::ThreadPool pool("MainThreadPool");
  pool.setMaxQueueSize(maxSize);
  pool.start(5);

  LOG_WARN << "Adding";
  pool.run(print);
  pool.run(print);
  for (int i = 0; i < 100; ++i)
  {
    char buf[32];
    snprintf(buf, sizeof buf, "task %d", i);
    pool.run(std::bind(printString, std::string(buf)));
  }
  LOG_WARN << "Done";

  jpmuduo::CountDownLatch latch(1);
  pool.run(std::bind(&jpmuduo::CountDownLatch::countDown, &latch));
  latch.wait();
  pool.stop();
}

void longTask(int num)
{
  LOG_INFO << "longTask " << num;
  usleep(3000*1000);
}

void test2()
{
  LOG_WARN << "Test ThreadPool by stoping early.";
  jpmuduo::ThreadPool pool("ThreadPool");
  pool.setMaxQueueSize(5);
  pool.start(3);

  jpmuduo::Thread thread1([&pool]()
  {
    for (int i = 0; i < 20; ++i)
    {
      pool.run(std::bind(longTask, i));
    }
  }, "thread1");
  thread1.start();

  usleep(5000*1000);
  LOG_WARN << "stop pool";
  pool.stop();  // early stop

  thread1.join();
  // run() after stop()
  pool.run(print);
  LOG_WARN << "test2 Done";
}

int main()
{
  test(0);
  test(1);
  test(5);
  test(10);
  test(50);
  test2();
  return 0;
}
