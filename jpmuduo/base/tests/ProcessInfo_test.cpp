// 对照原始 muduo ProcessInfo_test.cc 改写：
//   - namespace muduo -> jpmuduo
//   - jpmuduo 的 ProcessInfo 接口见 jpmuduo/base/ProcessInfo.h：
//     与原始 muduo 相比缺少 isDebugBuild 之外的其他打印项均存在
//     （本测试只用原始模板中出现的函数：pid/uid/euid/startTime/hostname/
//       openedFiles/threads/numThreads/procStatus，全部可用）
//   - 原始用 %zd 打印 size_t，这里统一用 %zu 避免符号匹配警告
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "jpmuduo/base/ProcessInfo.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>  // getuid / geteuid
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

int main()
{
  printf("pid = %d\n", jpmuduo::ProcessInfo::pid());
  printf("uid = %d\n", jpmuduo::ProcessInfo::uid());
  printf("euid = %d\n", jpmuduo::ProcessInfo::euid());
  printf("start time = %s\n", jpmuduo::ProcessInfo::startTime().toFormattedString().c_str());
  printf("hostname = %s\n", jpmuduo::ProcessInfo::hostname().c_str());
  printf("opened files = %d\n", jpmuduo::ProcessInfo::openedFiles());
  printf("threads = %zu\n", jpmuduo::ProcessInfo::threads().size());
  printf("num threads = %d\n", jpmuduo::ProcessInfo::numThreads());
  printf("status = %s\n", jpmuduo::ProcessInfo::procStatus().c_str());

  // 合理性检查
  assert(jpmuduo::ProcessInfo::pid() > 0);
  assert(jpmuduo::ProcessInfo::uid() == getuid());
  assert(jpmuduo::ProcessInfo::euid() == geteuid());
  assert(jpmuduo::ProcessInfo::numThreads() >= 1);
  assert(jpmuduo::ProcessInfo::threads().size() >= 1);
  assert(jpmuduo::ProcessInfo::openedFiles() >= 0);
  assert(!jpmuduo::ProcessInfo::hostname().empty());
  assert(!jpmuduo::ProcessInfo::procStatus().empty());

  printf("All passed.\n");
  return 0;
}
