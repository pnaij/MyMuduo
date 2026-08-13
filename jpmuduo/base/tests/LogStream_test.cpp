// 对照原始 muduo LogStream_test.cc 改写：
//   - 原始用 Boost.Test（BOOST_CHECK_EQUAL），本项目无 boost 依赖，
//     改为自包含 main() + assert/printf 风格
//   - muduo::string -> std::string
//   - 重点验证 formatInteger 的 itoa 正确性、SI/IEC 格式化、Fmt 宽度
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "jpmuduo/base/LogStream.h"

#include <limits>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

using jpmuduo::LogStream;
using jpmuduo::Fmt;

#define CHECK_EQ(a, b) \
  do { \
    const std::string __a = (a); \
    const std::string __b = (b); \
    if (__a != __b) { \
      printf("FAILED at line %d: \"%s\" != \"%s\"\n", __LINE__, __a.c_str(), __b.c_str()); \
      assert(__a == __b); \
    } \
  } while (0)

void testLogStreamBooleans()
{
  LogStream os;
  const LogStream::Buffer& buf = os.buffer();
  CHECK_EQ(buf.toString(), std::string(""));
  os << true;
  CHECK_EQ(buf.toString(), std::string("1"));
  os << '\n';
  CHECK_EQ(buf.toString(), std::string("1\n"));
  os << false;
  CHECK_EQ(buf.toString(), std::string("1\n0"));
}

void testLogStreamIntegers()
{
  LogStream os;
  const LogStream::Buffer& buf = os.buffer();
  CHECK_EQ(buf.toString(), std::string(""));
  os << 1;
  CHECK_EQ(buf.toString(), std::string("1"));
  os << 0;
  CHECK_EQ(buf.toString(), std::string("10"));
  os << -1;
  CHECK_EQ(buf.toString(), std::string("10-1"));
  os.resetBuffer();

  os << 0 << " " << 123 << 'x' << 0x64;
  CHECK_EQ(buf.toString(), std::string("0 123x100"));
}

void testLogStreamIntegerLimits()
{
  LogStream os;
  const LogStream::Buffer& buf = os.buffer();
  os << -2147483647;
  CHECK_EQ(buf.toString(), std::string("-2147483647"));
  os << static_cast<int>(-2147483647 - 1);
  CHECK_EQ(buf.toString(), std::string("-2147483647-2147483648"));
  os << ' ';
  os << 2147483647;
  CHECK_EQ(buf.toString(), std::string("-2147483647-2147483648 2147483647"));
  os.resetBuffer();

  os << std::numeric_limits<int16_t>::min();
  CHECK_EQ(buf.toString(), std::string("-32768"));
  os.resetBuffer();

  os << std::numeric_limits<int16_t>::max();
  CHECK_EQ(buf.toString(), std::string("32767"));
  os.resetBuffer();

  os << std::numeric_limits<uint16_t>::min();
  CHECK_EQ(buf.toString(), std::string("0"));
  os.resetBuffer();

  os << std::numeric_limits<uint16_t>::max();
  CHECK_EQ(buf.toString(), std::string("65535"));
  os.resetBuffer();

  os << std::numeric_limits<int32_t>::min();
  CHECK_EQ(buf.toString(), std::string("-2147483648"));
  os.resetBuffer();

  os << std::numeric_limits<int32_t>::max();
  CHECK_EQ(buf.toString(), std::string("2147483647"));
  os.resetBuffer();

  os << std::numeric_limits<uint32_t>::min();
  CHECK_EQ(buf.toString(), std::string("0"));
  os.resetBuffer();

  os << std::numeric_limits<uint32_t>::max();
  CHECK_EQ(buf.toString(), std::string("4294967295"));
  os.resetBuffer();

  os << std::numeric_limits<int64_t>::min();
  CHECK_EQ(buf.toString(), std::string("-9223372036854775808"));
  os.resetBuffer();

  os << std::numeric_limits<int64_t>::max();
  CHECK_EQ(buf.toString(), std::string("9223372036854775807"));
  os.resetBuffer();

  os << std::numeric_limits<uint64_t>::min();
  CHECK_EQ(buf.toString(), std::string("0"));
  os.resetBuffer();

  os << std::numeric_limits<uint64_t>::max();
  CHECK_EQ(buf.toString(), std::string("18446744073709551615"));
  os.resetBuffer();

  int16_t a = 0;
  int32_t b = 0;
  int64_t c = 0;
  os << a;
  os << b;
  os << c;
  CHECK_EQ(buf.toString(), std::string("000"));
}

void testLogStreamFloats()
{
  LogStream os;
  const LogStream::Buffer& buf = os.buffer();

  os << 0.0;
  CHECK_EQ(buf.toString(), std::string("0"));
  os.resetBuffer();

  os << 1.0;
  CHECK_EQ(buf.toString(), std::string("1"));
  os.resetBuffer();

  os << 0.1;
  CHECK_EQ(buf.toString(), std::string("0.1"));
  os.resetBuffer();

  os << 0.05;
  CHECK_EQ(buf.toString(), std::string("0.05"));
  os.resetBuffer();

  os << 0.15;
  CHECK_EQ(buf.toString(), std::string("0.15"));
  os.resetBuffer();

  double a = 0.1;
  os << a;
  CHECK_EQ(buf.toString(), std::string("0.1"));
  os.resetBuffer();

  double b = 0.05;
  os << b;
  CHECK_EQ(buf.toString(), std::string("0.05"));
  os.resetBuffer();

  double c = 0.15;
  os << c;
  CHECK_EQ(buf.toString(), std::string("0.15"));
  os.resetBuffer();

  os << a+b;
  CHECK_EQ(buf.toString(), std::string("0.15"));
  os.resetBuffer();

  assert(a+b != c);

  os << 1.23456789;
  CHECK_EQ(buf.toString(), std::string("1.23456789"));
  os.resetBuffer();

  os << 1.234567;
  CHECK_EQ(buf.toString(), std::string("1.234567"));
  os.resetBuffer();

  os << -123.456;
  CHECK_EQ(buf.toString(), std::string("-123.456"));
  os.resetBuffer();
}

void testLogStreamVoid()
{
  LogStream os;
  const LogStream::Buffer& buf = os.buffer();

  os << static_cast<void*>(0);
  CHECK_EQ(buf.toString(), std::string("0x0"));
  os.resetBuffer();

  os << reinterpret_cast<void*>(8888);
  CHECK_EQ(buf.toString(), std::string("0x22B8"));
  os.resetBuffer();
}

void testLogStreamStrings()
{
  LogStream os;
  const LogStream::Buffer& buf = os.buffer();

  os << "Hello ";
  CHECK_EQ(buf.toString(), std::string("Hello "));

  std::string chenshuo = "Shuo Chen";
  os << chenshuo;
  CHECK_EQ(buf.toString(), std::string("Hello Shuo Chen"));
}

void testLogStreamFmts()
{
  LogStream os;
  const LogStream::Buffer& buf = os.buffer();

  os << Fmt("%4d", 1);
  CHECK_EQ(buf.toString(), std::string("   1"));
  os.resetBuffer();

  os << Fmt("%4.2f", 1.2);
  CHECK_EQ(buf.toString(), std::string("1.20"));
  os.resetBuffer();

  os << Fmt("%4.2f", 1.2) << Fmt("%4d", 43);
  CHECK_EQ(buf.toString(), std::string("1.20  43"));
  os.resetBuffer();
}

void testLogStreamLong()
{
  LogStream os;
  const LogStream::Buffer& buf = os.buffer();
  for (int i = 0; i < 399; ++i)
  {
    os << "123456789 ";
    assert(buf.length() == 10*(i+1));
    assert(buf.avail() == 4000 - 10*(i+1));
  }

  os << "abcdefghi ";
  assert(buf.length() == 3990);
  assert(buf.avail() == 10);

  os << "abcdefghi";
  assert(buf.length() == 3999);
  assert(buf.avail() == 1);
}

void testFormatSI()
{
  CHECK_EQ(jpmuduo::formatSI(0), std::string("0"));
  CHECK_EQ(jpmuduo::formatSI(999), std::string("999"));
  CHECK_EQ(jpmuduo::formatSI(1000), std::string("1.00k"));
  CHECK_EQ(jpmuduo::formatSI(9990), std::string("9.99k"));
  CHECK_EQ(jpmuduo::formatSI(9994), std::string("9.99k"));
  CHECK_EQ(jpmuduo::formatSI(9995), std::string("10.0k"));
  CHECK_EQ(jpmuduo::formatSI(10000), std::string("10.0k"));
  CHECK_EQ(jpmuduo::formatSI(10049), std::string("10.0k"));
  CHECK_EQ(jpmuduo::formatSI(10050), std::string("10.1k"));
  CHECK_EQ(jpmuduo::formatSI(99900), std::string("99.9k"));
  CHECK_EQ(jpmuduo::formatSI(99949), std::string("99.9k"));
  CHECK_EQ(jpmuduo::formatSI(99950), std::string("100k"));
  CHECK_EQ(jpmuduo::formatSI(100499), std::string("100k"));
  CHECK_EQ(jpmuduo::formatSI(100501), std::string("101k"));
  CHECK_EQ(jpmuduo::formatSI(999499), std::string("999k"));
  CHECK_EQ(jpmuduo::formatSI(999500), std::string("1.00M"));
  CHECK_EQ(jpmuduo::formatSI(1004999), std::string("1.00M"));
  CHECK_EQ(jpmuduo::formatSI(1005001), std::string("1.01M"));
  CHECK_EQ(jpmuduo::formatSI(INT64_MAX), std::string("9.22E"));
}

void testFormatIEC()
{
  CHECK_EQ(jpmuduo::formatIEC(0), std::string("0"));
  CHECK_EQ(jpmuduo::formatIEC(1023), std::string("1023"));
  CHECK_EQ(jpmuduo::formatIEC(1024), std::string("1.00Ki"));
  CHECK_EQ(jpmuduo::formatIEC(10234), std::string("9.99Ki"));
  CHECK_EQ(jpmuduo::formatIEC(10235), std::string("10.0Ki"));
  CHECK_EQ(jpmuduo::formatIEC(10240), std::string("10.0Ki"));
  CHECK_EQ(jpmuduo::formatIEC(10291), std::string("10.0Ki"));
  CHECK_EQ(jpmuduo::formatIEC(10292), std::string("10.1Ki"));
  CHECK_EQ(jpmuduo::formatIEC(102348), std::string("99.9Ki"));
  CHECK_EQ(jpmuduo::formatIEC(102349), std::string("100Ki"));
  CHECK_EQ(jpmuduo::formatIEC(102912), std::string("100Ki"));
  CHECK_EQ(jpmuduo::formatIEC(102913), std::string("101Ki"));
  CHECK_EQ(jpmuduo::formatIEC(1022976), std::string("999Ki"));
  CHECK_EQ(jpmuduo::formatIEC(1047552), std::string("1023Ki"));
  CHECK_EQ(jpmuduo::formatIEC(1047961), std::string("1023Ki"));
  CHECK_EQ(jpmuduo::formatIEC(1048063), std::string("1023Ki"));
  CHECK_EQ(jpmuduo::formatIEC(1048064), std::string("1.00Mi"));
  CHECK_EQ(jpmuduo::formatIEC(1048576), std::string("1.00Mi"));
  CHECK_EQ(jpmuduo::formatIEC(10480517), std::string("9.99Mi"));
  CHECK_EQ(jpmuduo::formatIEC(10480518), std::string("10.0Mi"));
  CHECK_EQ(jpmuduo::formatIEC(INT64_MAX), std::string("8.00Ei"));
}

int main()
{
  testLogStreamBooleans();
  testLogStreamIntegers();
  testLogStreamIntegerLimits();
  testLogStreamFloats();
  testLogStreamVoid();
  testLogStreamStrings();
  testLogStreamFmts();
  testLogStreamLong();
  testFormatSI();
  testFormatIEC();
  printf("All passed.\n");
  return 0;
}
