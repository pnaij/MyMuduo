// 对照原始 muduo Date_unittest.cc 改写：
//   - namespace muduo -> jpmuduo，Date::toString() -> Date::toIsoString()
//   - 项目编译选项带 -DNDEBUG，先取消以保证 assert 校验生效
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "jpmuduo/base/Date.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

using jpmuduo::Date;

const int kMonthsOfYear = 12;

int isLeapYear(int year)
{
  if (year % 400 == 0)
    return 1;
  else if (year % 100 == 0)
    return 0;
  else if (year % 4 == 0)
    return 1;
  else
    return 0;
}

int daysOfMonth(int year, int month)
{
  static int days[2][kMonthsOfYear+1] =
  {
    { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  };
  return days[isLeapYear(year)][month];
}

void passByConstReference(const Date& x)
{
  printf("%s\n", x.toIsoString().c_str());
}

void passByValue(Date x)
{
  printf("%s\n", x.toIsoString().c_str());
}

int main()
{
  time_t now = time(NULL);
  struct tm t1 = *gmtime(&now);
  struct tm t2 = *localtime(&now);
  Date someDay(2008, 9, 10);
  printf("%s\n", someDay.toIsoString().c_str());
  passByValue(someDay);
  passByConstReference(someDay);
  Date todayUtc(t1);
  printf("%s\n", todayUtc.toIsoString().c_str());
  Date todayLocal(t2);
  printf("%s\n", todayLocal.toIsoString().c_str());

  // 1900~2500 年逐日校验 JDN 往返转换与 weekDay 递增
  int julianDayNumber = 2415021;  // 1900-01-01
  int weekDay = 1;                // Monday

  for (int year = 1900; year < 2500; ++year)
  {
    assert(Date(year, 3, 1).julianDayNumber() - Date(year, 2, 29).julianDayNumber()
           == isLeapYear(year));
    for (int month = 1; month <= kMonthsOfYear; ++month)
    {
      for (int day = 1; day <= daysOfMonth(year, month); ++day)
      {
        Date d(year, month, day);
        assert(year == d.year());
        assert(month == d.month());
        assert(day == d.day());
        assert(weekDay == d.weekDay());
        assert(julianDayNumber == d.julianDayNumber());

        Date d2(julianDayNumber);
        assert(year == d2.year());
        assert(month == d2.month());
        assert(day == d2.day());
        assert(weekDay == d2.weekDay());
        assert(julianDayNumber == d2.julianDayNumber());

        ++julianDayNumber;
        weekDay = (weekDay+1) % 7;
      }
    }
  }
  printf("All passed.\n");
  return 0;
}
