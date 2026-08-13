// 对照原始 muduo Singleton_test.cc 改写：
//   - namespace muduo -> jpmuduo，muduo::string -> std::string
//   - jpmuduo 无 CurrentThread::name()，只用 CurrentThread::tid()（模板本就用 tid）
#include "jpmuduo/base/Singleton.h"
#include "jpmuduo/base/CurrentThread.h"
#include "jpmuduo/base/Thread.h"

#include <stdio.h>
#include <string>

class Test : jpmuduo::noncopyable
{
 public:
  Test()
  {
    printf("tid=%d, constructing %p\n", jpmuduo::CurrentThread::tid(), this);
  }

  ~Test()
  {
    printf("tid=%d, destructing %p %s\n", jpmuduo::CurrentThread::tid(), this, name_.c_str());
  }

  const std::string& name() const { return name_; }
  void setName(const std::string& n) { name_ = n; }

 private:
  std::string name_;
};

class TestNoDestroy : jpmuduo::noncopyable
{
 public:
  // Tag member for Singleton<T>
  void no_destroy();

  TestNoDestroy()
  {
    printf("tid=%d, constructing TestNoDestroy %p\n", jpmuduo::CurrentThread::tid(), this);
  }

  ~TestNoDestroy()
  {
    printf("tid=%d, destructing TestNoDestroy %p\n", jpmuduo::CurrentThread::tid(), this);
  }
};

void threadFunc()
{
  printf("tid=%d, %p name=%s\n",
         jpmuduo::CurrentThread::tid(),
         &jpmuduo::Singleton<Test>::instance(),
         jpmuduo::Singleton<Test>::instance().name().c_str());
  jpmuduo::Singleton<Test>::instance().setName("only one, changed");
}

int main()
{
  jpmuduo::Singleton<Test>::instance().setName("only one");
  jpmuduo::Thread t1(threadFunc);
  t1.start();
  t1.join();
  printf("tid=%d, %p name=%s\n",
         jpmuduo::CurrentThread::tid(),
         &jpmuduo::Singleton<Test>::instance(),
         jpmuduo::Singleton<Test>::instance().name().c_str());
  jpmuduo::Singleton<TestNoDestroy>::instance();
  printf("with valgrind, you should see %zd-byte memory leak.\n", sizeof(TestNoDestroy));
  return 0;
}
