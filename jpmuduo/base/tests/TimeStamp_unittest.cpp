// 对照原始 muduo Timestamp_unittest.cc 改写：
//   - muduo::Timestamp -> jpmuduo::TimeStamp（jpmuduo 类名大小写不同）
//   - jpmuduo 没有 timeDifference() 自由函数，用 microSecondsSinceEpoch() 相减
//   - jpmuduo 的 TimeStamp 以 CLOCK_MONOTONIC 为基准，toString() 输出的是
//     开机以来的秒数（不是 epoch 秒数），因此测试不做 epoch 相关断言；
//     toFormattedString() 输出的是墙钟时间
#include "jpmuduo/base/TimeStamp.h"
#include <vector>
#include <stdio.h>

using jpmuduo::TimeStamp;

void passByConstReference(const TimeStamp& x)
{
  printf("%s\n", x.toString().c_str());
}

void passByValue(TimeStamp x)
{
  printf("%s\n", x.toString().c_str());
}

void benchmark()
{
  const int kNumber = 1000*1000;

  std::vector<TimeStamp> stamps;
  stamps.reserve(kNumber);
  for (int i = 0; i < kNumber; ++i)
  {
    stamps.push_back(TimeStamp::now());
  }
  printf("%s\n", stamps.front().toString().c_str());
  printf("%s\n", stamps.back().toString().c_str());
  // 100 万次 now() 的总耗时（秒）
  printf("%f\n", (stamps.back().microSecondsSinceEpoch()
                  - stamps.front().microSecondsSinceEpoch()) / 1000000.0);

  // 检查 now() 的读数单调递增
  int increments[100] = { 0 };
  int64_t start = stamps.front().microSecondsSinceEpoch();
  for (int i = 1; i < kNumber; ++i)
  {
    int64_t next = stamps[i].microSecondsSinceEpoch();
    int64_t inc = next - start;
    start = next;
    if (inc < 0)
    {
      printf("reverse!\n");
    }
    else if (inc < 100)
    {
      ++increments[inc];
    }
    else
    {
      printf("big gap %d\n", static_cast<int>(inc));
    }
  }

  for (int i = 0; i < 100; ++i)
  {
    printf("%2d: %d\n", i, increments[i]);
  }
}

int main()
{
  TimeStamp now(TimeStamp::now());
  printf("%s\n", now.toString().c_str());
  passByValue(now);
  passByConstReference(now);
  // toFormattedString() 输出墙钟时间（格式 yyyy-mm-dd hh:mm:ss.ffffff）
  printf("%s\n", now.toFormattedString().c_str());
  benchmark();
  return 0;
}
