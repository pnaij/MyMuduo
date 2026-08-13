// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef JPMUDUO_LOGGING_H
#define JPMUDUO_LOGGING_H

#include "jpmuduo/base/LogStream.h"
#include "jpmuduo/base/TimeStamp.h"

#include <functional>

namespace jpmuduo
{

class TimeZone;

class Logger
{
 public:
  enum LogLevel
  {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
    NUM_LOG_LEVELS,
  };

  class SourceFile
  {
   public:
    template<int N>
    SourceFile(const char (&arr)[N])
      : data_(arr),
        size_(N-1)
    {
      const char* slash = strrchr(data_, '/');
      if (slash)
      {
        data_ = slash + 1;
        size_ -= static_cast<int>(data_ - arr);
      }
    }

    explicit SourceFile(const char* filename)
      : data_(filename)
    {
      const char* slash = strrchr(filename, '/');
      if (slash)
      {
        data_ = slash + 1;
      }
      size_ = static_cast<int>(strlen(data_));
    }

    const char* data_;
    int size_;
  };

  Logger(SourceFile file, int line);
  Logger(SourceFile file, int line, LogLevel level);
  Logger(SourceFile file, int line, LogLevel level, const char* func);
  Logger(SourceFile file, int line, bool toAbort);
  ~Logger();

  LogStream& stream() { return impl_.stream_; }

  static LogLevel logLevel();
  static void setLogLevel(LogLevel level);

  typedef std::function<void(const char* msg, int len)> OutputFunc;
  typedef std::function<void()> FlushFunc;
  static void setOutput(OutputFunc);
  static void setFlush(FlushFunc);
  static void setTimeZone(const TimeZone& tz);

 private:

class Impl
{
 public:
  typedef Logger::LogLevel LogLevel;
  Impl(LogLevel level, int old_errno, const SourceFile& file, int line);
  void formatTime();
  void finish();

  TimeStamp time_;
  LogStream stream_;
  LogLevel level_;
  int line_;
  SourceFile basename_;
};

  Impl impl_;
};

extern Logger::LogLevel g_logLevel;

inline Logger::LogLevel Logger::logLevel()
{
  return g_logLevel;
}

#define LOG_TRACE if (jpmuduo::Logger::logLevel() <= jpmuduo::Logger::TRACE) \
  jpmuduo::Logger(__FILE__, __LINE__, jpmuduo::Logger::TRACE, __func__).stream()
#define LOG_DEBUG if (jpmuduo::Logger::logLevel() <= jpmuduo::Logger::DEBUG) \
  jpmuduo::Logger(__FILE__, __LINE__, jpmuduo::Logger::DEBUG, __func__).stream()
#define LOG_INFO if (jpmuduo::Logger::logLevel() <= jpmuduo::Logger::INFO) \
  jpmuduo::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN jpmuduo::Logger(__FILE__, __LINE__, jpmuduo::Logger::WARN).stream()
#define LOG_ERROR jpmuduo::Logger(__FILE__, __LINE__, jpmuduo::Logger::ERROR).stream()
#define LOG_FATAL jpmuduo::Logger(__FILE__, __LINE__, jpmuduo::Logger::FATAL).stream()
#define LOG_SYSERR jpmuduo::Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL jpmuduo::Logger(__FILE__, __LINE__, true).stream()

const char* strerror_tl(int savedErrno);

#define CHECK_NOTNULL(val) \
  ::jpmuduo::CheckNotNull(__FILE__, __LINE__, "'" #val "' Must be non NULL", (val))

template <typename T>
T* CheckNotNull(Logger::SourceFile file, int line, const char *names, T* ptr)
{
  if (ptr == NULL)
  {
   Logger(file, line, Logger::FATAL).stream() << names;
  }
  return ptr;
}

}  // namespace jpmuduo

#endif  // JPMUDUO_LOGGING_H