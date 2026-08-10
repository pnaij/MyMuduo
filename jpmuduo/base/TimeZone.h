// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef JPMUDUO_TIMEZONE_H
#define JPMUDUO_TIMEZONE_H

#include "jpmuduo/base/copyable.h"

#include <memory>
#include <time.h>

namespace jpmuduo
{

struct DateTime
{
  DateTime() {}
  explicit DateTime(const struct tm&);
  DateTime(int _year, int _month, int _day, int _hour, int _minute, int _second)
      : year(_year), month(_month), day(_day), hour(_hour), minute(_minute), second(_second)
  {
  }

  std::string toIsoString() const;

  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
};

// Simplified TimeZone: supports UTC and China(GMT+8) only.
// Does NOT parse zoneinfo files.
class TimeZone : public copyable
{
 public:
  TimeZone() = default;  // an invalid timezone
  TimeZone(int eastOfUtc, const char* tzname);  // a fixed timezone

  static TimeZone UTC();
  static TimeZone China();  // Fixed at GMT+8, no DST

  bool valid() const
  {
    return static_cast<bool>(data_);
  }

  struct DateTime toLocalTime(int64_t secondsSinceEpoch, int* utcOffset = nullptr) const;
  int64_t fromLocalTime(const struct DateTime&, bool postTransition = false) const;

  static struct DateTime toUtcTime(int64_t secondsSinceEpoch);
  static int64_t fromUtcTime(const struct DateTime&);

  struct Data;

 private:
  explicit TimeZone(std::unique_ptr<Data> data);

  std::shared_ptr<Data> data_;
};

}  // namespace jpmuduo

#endif  // JPMUDUO_TIMEZONE_H