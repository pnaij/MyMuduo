// 对照原始 muduo AsyncLogging_test.cc 改写：
//   - namespace muduo -> jpmuduo
//   - 原始模板无 timeDifference()，已简化为 jpmuduo 版本（只打印耗时）
//   - jpmuduo 的 AsyncLogging 只有 append/start/stop（无 flush()），
//     线程在退出前负责把缓冲区落盘
//   - 按任务要求简化 bench：写批量日志后 stop()，确认日志文件存在且有内容
//   - 原始模板用 setrlimit 限制虚拟内存，测试无此必要，已省略
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "jpmuduo/base/AsyncLogging.h"
#include "jpmuduo/base/Logging.h"
#include "jpmuduo/base/TimeStamp.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <vector>

off_t kRollSize = 500*1000*1000;  // 500MB，避免本测试触发滚动

jpmuduo::AsyncLogging* g_asyncLog = NULL;

void asyncOutput(const char* msg, int len)
{
  g_asyncLog->append(msg, len);
}

// 收集当前目录下 basename 前缀、.log 后缀的文件
std::vector<std::string> listLogFiles(const std::string& basename)
{
  std::vector<std::string> result;
  DIR* dir = ::opendir(".");
  assert(dir != NULL);
  struct dirent* entry;
  while ((entry = ::readdir(dir)) != NULL)
  {
    std::string name(entry->d_name);
    if (name.compare(0, basename.size(), basename) == 0
        && name.size() > 4
        && name.compare(name.size()-4, 4, ".log") == 0)
    {
      result.push_back(name);
    }
  }
  ::closedir(dir);
  return result;
}

int main(int argc, char* argv[])
{
  printf("pid = %d\n", getpid());

  char name[256] = { '\0' };
  strncpy(name, argv[0], sizeof name - 1);
  std::string basename(::basename(name));

  jpmuduo::AsyncLogging log(basename, kRollSize);
  log.start();
  g_asyncLog = &log;

  jpmuduo::Logger::setOutput(asyncOutput);

  // 批量写日志：20 轮 * 1000 条 = 2 万条
  int cnt = 0;
  const int kBatch = 1000;
  std::string empty = " ";
  std::string longStr(3000, 'X');
  longStr += " ";

  bool longLog = argc > 1;
  jpmuduo::TimeStamp start = jpmuduo::TimeStamp::now();
  for (int t = 0; t < 20; ++t)
  {
    for (int i = 0; i < kBatch; ++i)
    {
      LOG_INFO << "Hello 0123456789" << " abcdefghijklmnopqrstuvwxyz "
               << (longLog ? longStr : empty)
               << cnt;
      ++cnt;
    }
  }
  jpmuduo::TimeStamp end = jpmuduo::TimeStamp::now();
  printf("wrote %d lines in %f s\n", cnt,
         (end.microSecondsSinceEpoch() - start.microSecondsSinceEpoch()) / 1000000.0);

  // stop() 会 join 后台线程，线程退出前把最后一批缓冲区写入文件
  log.stop();

  std::vector<std::string> files = listLogFiles(basename);
  printf("%zd log file(s) generated:\n", files.size());
  for (const auto& f : files)
  {
    printf("  %s\n", f.c_str());
  }

  // 日志文件必须存在且有内容
  assert(files.size() >= 1);
  bool hasContent = false;
  bool hasHello = false;
  for (const auto& f : files)
  {
    FILE* fp = ::fopen(f.c_str(), "rb");
    if (fp == NULL)
    {
      continue;
    }
    char buf[64*1024];
    size_t n = ::fread(buf, 1, sizeof buf, fp);
    ::fclose(fp);
    std::string content(buf, n);
    if (n > 0)
    {
      hasContent = true;
    }
    if (content.find("Hello 0123456789") != std::string::npos)
    {
      hasHello = true;
    }
  }
  assert(hasContent);
  assert(hasHello);

  printf("All passed.\n");
  return 0;
}
