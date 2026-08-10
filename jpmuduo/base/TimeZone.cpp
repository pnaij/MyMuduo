// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "jpmuduo/base/TimeZone.h"
#include "jpmuduo/base/Date.h"

#include <assert.h>
#include <stdio.h>
#include <string>

namespace jpmuduo
{

struct TimeZone::Data
{
  int utcOffset;  // seconds east of UTC
  const char* name;
};

const int kSecondsPerDay = 24 * 60 * 60;

namespace detail
{

// Break seconds since epoch into DateTime (UTC)
DateTime BreakTime(int64_t secondsSinceEpoch)
{
  // Calculate seconds within the day
  int64_t seconds = secondsSinceEpoch;
  int second = static_cast<int>(seconds % 60);
  seconds /= 60;
  int minute = static_cast<int>(seconds % 60);
  seconds /= 60;
  int hour = static_cast<int>(seconds % 24);
  seconds /= 24;  // now seconds is days since epoch

  // Convert days to date using Julian Day Number
  int jdn = static_cast<int>(seconds + Date::kJulianDayOf1970_01_01);
  Date date(jdn);
  return DateTime(date.year(), date.month(), date.day(), hour, minute, second);
}

}  // namespace detail

}  // namespace jpmuduo

using namespace jpmuduo;

TimeZone::TimeZone(int eastOfUtc, const char* tzname)
  : data_(new Data{eastOfUtc, tzname})
{
}

TimeZone::TimeZone(std::unique_ptr<Data> data)
  : data_(std::move(data))
{
}

TimeZone TimeZone::UTC()
{
  return TimeZone(0, "UTC");
}

TimeZone TimeZone::China()
{
  return TimeZone(8 * 3600, "CST");
}

DateTime TimeZone::toUtcTime(int64_t secondsSinceEpoch)
{
  return detail::BreakTime(secondsSinceEpoch);
}

int64_t TimeZone::fromUtcTime(const DateTime& dt)
{
  Date date(dt.year, dt.month, dt.day);
  int secondsInDay = dt.hour * 3600 + dt.minute * 60 + dt.second;
  int64_t days = date.julianDayNumber() - Date::kJulianDayOf1970_01_01;
  return days * kSecondsPerDay + secondsInDay;
}

DateTime TimeZone::toLocalTime(int64_t seconds, int* utcOffset) const
{
  DateTime localTime;
  assert(data_ != NULL);

  int offset = data_->utcOffset;
  localTime = detail::BreakTime(seconds + offset);
  if (utcOffset)
  {
    *utcOffset = offset;
  }
  return localTime;
}

int64_t TimeZone::fromLocalTime(const DateTime& localtime, bool /*postTransition*/) const
{
  assert(data_ != NULL);
  int64_t localSeconds = fromUtcTime(localtime);
  return localSeconds - data_->utcOffset;
}

DateTime::DateTime(const struct tm& t)
  : year(t.tm_year + 1900), month(t.tm_mon + 1), day(t.tm_mday),
    hour(t.tm_hour), minute(t.tm_min), second(t.tm_sec)
{
}

std::string DateTime::toIsoString() const
{
  char buf[64];
  snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d",
           year, month, day, hour, minute, second);
  return buf;
}