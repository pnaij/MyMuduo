// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef JPMUDUO_PROCESSINFO_H
#define JPMUDUO_PROCESSINFO_H

#include "jpmuduo/base/StringPiece.h"
#include "jpmuduo/base/TimeStamp.h"
#include <vector>
#include <sys/types.h>

namespace jpmuduo
{

namespace ProcessInfo
{
  pid_t pid();
  std::string pidString();
  uid_t uid();
  std::string username();
  uid_t euid();
  TimeStamp startTime();
  int clockTicksPerSecond();
  int pageSize();
  bool isDebugBuild();

  std::string hostname();
  std::string procname();
  StringPiece procname(const std::string& stat);

  /// read /proc/self/status
  std::string procStatus();

  /// read /proc/self/stat
  std::string procStat();

  /// read /proc/self/task/tid/stat
  std::string threadStat();

  /// readlink /proc/self/exe
  std::string exePath();

  int openedFiles();
  int maxOpenFiles();

  struct CpuTime
  {
    double userSeconds;
    double systemSeconds;

    CpuTime() : userSeconds(0.0), systemSeconds(0.0) { }

    double total() const { return userSeconds + systemSeconds; }
  };
  CpuTime cpuTime();

  int numThreads();
  std::vector<pid_t> threads();
}  // namespace ProcessInfo

}  // namespace jpmuduo

#endif  // JPMUDUO_PROCESSINFO_H