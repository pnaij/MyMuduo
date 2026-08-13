// 对照原始 muduo LogFile_test.cc 改写：
//   - namespace muduo -> jpmuduo，muduo::string -> std::string
//   - 原始模板写 10000 行后直接退出；按任务要求，运行结束后检查
//     当前目录生成的 *.log 文件数量 > 1（触发滚动）且内容正确
//   - 原始模板 usleep(1000)/行；jpmuduo 的 rollFile() 仅在跨秒时（now > lastRoll_）
//     才真正滚动，若写太快（1 万行同一秒内写完）不会触发滚动，
//     因此保留少量延时（usleep(500)）保证测试跨秒、滚动正常发生
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "jpmuduo/base/LogFile.h"
#include "jpmuduo/base/Logging.h"
#include "jpmuduo/base/ProcessInfo.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <vector>

std::unique_ptr<jpmuduo::LogFile> g_logFile;

void outputFunc(const char* msg, int len)
{
  g_logFile->append(msg, len);
}

void flushFunc()
{
  g_logFile->flush();
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

// 检查所有日志文件的总内容是否包含指定文本
bool allContentContains(const std::vector<std::string>& files,
                        const std::string& needle)
{
  for (const auto& f : files)
  {
    FILE* fp = ::fopen(f.c_str(), "rb");
    if (fp == NULL)
    {
      return false;
    }
    std::string content;
    char buf[64*1024];
    size_t n;
    while ((n = ::fread(buf, 1, sizeof buf, fp)) > 0)
    {
      content.append(buf, n);
    }
    ::fclose(fp);
    if (content.find(needle) != std::string::npos)
    {
      return true;
    }
  }
  return false;
}

int main(int argc, char* argv[])
{
  char name[256] = { '\0' };
  strncpy(name, argv[0], sizeof name - 1);
  std::string basename(::basename(name));
  g_logFile.reset(new jpmuduo::LogFile(basename, 200*1000));
  jpmuduo::Logger::setOutput(outputFunc);
  jpmuduo::Logger::setFlush(flushFunc);

  std::string line = "1234567890 abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

  for (int i = 0; i < 10000; ++i)
  {
    LOG_INFO << line << i;
    usleep(500);
  }
  g_logFile->flush();
  g_logFile.reset();  // 析构落盘，保证退出前文件内容完整

  std::vector<std::string> files = listLogFiles(basename);
  printf("%zd log file(s) generated:\n", files.size());
  for (const auto& f : files)
  {
    printf("  %s\n", f.c_str());
  }

  // 10000 行 * 约 75 字节 = 约 750KB，rollSize 200KB，应滚动出多个文件
  assert(files.size() > 1);
  // 内容正确性：首行特征文本存在，且最后一行的序号 9999 已落盘
  assert(allContentContains(files, "1234567890 abcdefghijklmnopqrstuvwxyz"));
  assert(allContentContains(files, "9999"));

  printf("All passed.\n");
  return 0;
}
