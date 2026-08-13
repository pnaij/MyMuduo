// Buffer_unittest.cpp
//
// 仿照 muduo/muduo/net/tests/Buffer_unittest.cc 重写（去掉 boost，改为
// 自包含 main + CHECK 断言），针对 jpmuduo::Buffer 验证：
//   - readableBytes / writableBytes / prependableBytes 一致性
//   - append / retrieve / retrieveAll 系列
//   - 自动扩容（makeSpace 的 resize 分支与内部搬运分支）
//   - shrink、prepend、internalCapacity
//   - readInt*/writeInt*/peekInt* 系列（含边界值）与网络字节序往返
//   - findEOL / 移动构造

#include "jpmuduo/net/Buffer.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

using jpmuduo::Buffer;
using std::string;

static int g_failures = 0;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #expr);                                        \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static void testBufferAppendRetrieve() {
    Buffer buf;
    CHECK(buf.readableBytes() == 0);
    CHECK(buf.writableBytes() == Buffer::kInitialSize);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend);

    const string str(200, 'x');
    buf.append(str.data(), str.size());
    CHECK(buf.readableBytes() == str.size());
    CHECK(buf.writableBytes() == Buffer::kInitialSize - str.size());
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend);

    const string str2 = buf.retrieveAsString(50);
    CHECK(str2.size() == 50);
    CHECK(buf.readableBytes() == str.size() - str2.size());
    CHECK(buf.writableBytes() == Buffer::kInitialSize - str.size());
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend + str2.size());
    CHECK(str2 == string(50, 'x'));

    buf.append(str.data(), str.size());
    CHECK(buf.readableBytes() == 2 * str.size() - str2.size());
    CHECK(buf.writableBytes() == Buffer::kInitialSize - 2 * str.size());
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend + str2.size());

    const string str3 = buf.retrieveAllAsString();
    CHECK(str3.size() == 350);
    CHECK(buf.readableBytes() == 0);
    CHECK(buf.writableBytes() == Buffer::kInitialSize);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend);
    CHECK(str3 == string(350, 'x'));

    // peek() 应指向可读数据起点
    buf.append("hello", 5);
    CHECK(memcmp(buf.peek(), "hello", 5) == 0);
    CHECK(buf.peek() + buf.readableBytes() == buf.beginWirte());
}

static void testBufferGrow() {
    Buffer buf;
    buf.append(string(400, 'y').data(), 400);
    CHECK(buf.readableBytes() == 400);
    CHECK(buf.writableBytes() == Buffer::kInitialSize - 400);

    buf.retrieve(50);
    CHECK(buf.readableBytes() == 350);
    CHECK(buf.writableBytes() == Buffer::kInitialSize - 400);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend + 50);

    // 触发 makeSpace 的 resize 分支：writable + prependable 都不够用
    buf.append(string(1000, 'z').data(), 1000);
    CHECK(buf.readableBytes() == 1350);
    CHECK(buf.writableBytes() == 0);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend + 50);  // FIXME(同原版)

    buf.retrieveAll();
    CHECK(buf.readableBytes() == 0);
    CHECK(buf.writableBytes() == 1400);  // FIXME(同原版)
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend);

    // internalCapacity 应 >= 当前缓冲总大小（readable+writable+prependable）
    CHECK(buf.internalCapacity() >= buf.readableBytes() + buf.writableBytes() +
                                       buf.prependableBytes());
}

static void testBufferInsideGrow() {
    Buffer buf;
    buf.append(string(800, 'y').data(), 800);
    CHECK(buf.readableBytes() == 800);
    CHECK(buf.writableBytes() == Buffer::kInitialSize - 800);

    buf.retrieve(500);
    CHECK(buf.readableBytes() == 300);
    CHECK(buf.writableBytes() == Buffer::kInitialSize - 800);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend + 500);

    // 触发 makeSpace 的内部搬运分支：把可读数据搬到前面腾出写空间
    buf.append(string(300, 'z').data(), 300);
    CHECK(buf.readableBytes() == 600);
    CHECK(buf.writableBytes() == Buffer::kInitialSize - 600);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend);

    // 搬运后数据仍然完整
    const string all = buf.retrieveAllAsString();
    CHECK(all.size() == 600);
    CHECK(all.substr(0, 300) == string(300, 'y'));
    CHECK(all.substr(300) == string(300, 'z'));
}

static void testBufferShrink() {
    Buffer buf;
    buf.append(string(2000, 'y').data(), 2000);
    CHECK(buf.readableBytes() == 2000);
    CHECK(buf.writableBytes() == 0);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend);
    size_t capBefore = buf.internalCapacity();

    buf.retrieve(1500);
    CHECK(buf.readableBytes() == 500);
    CHECK(buf.writableBytes() == 0);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend + 1500);

    buf.shrink(0);
    CHECK(buf.readableBytes() == 500);
    CHECK(buf.writableBytes() == Buffer::kInitialSize - 500);
    CHECK(buf.retrieveAllAsString() == string(500, 'y'));
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend);
    CHECK(buf.internalCapacity() <= capBefore);

    // shrink 后总容量依然 >= 8 + 可读数据
    CHECK(buf.internalCapacity() >= Buffer::kCheapPrepend + 500);
}

static void testBufferPrepend() {
    Buffer buf;
    buf.append(string(200, 'y').data(), 200);
    CHECK(buf.readableBytes() == 200);
    CHECK(buf.writableBytes() == Buffer::kInitialSize - 200);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend);

    int x = 0;
    buf.prepend(&x, sizeof x);
    CHECK(buf.readableBytes() == 204);
    CHECK(buf.writableBytes() == Buffer::kInitialSize - 200);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend - 4);
}

static void testBufferReadInt() {
    Buffer buf;
    buf.append("HTTP", 4);

    CHECK(buf.readableBytes() == 4);
    CHECK(buf.peekInt8() == 'H');
    const int top16 = buf.peekInt16();
    CHECK(top16 == 'H' * 256 + 'T');
    CHECK(buf.peekInt32() == top16 * 65536 + 'T' * 256 + 'P');

    // peek 不消耗数据
    CHECK(buf.readableBytes() == 4);
    CHECK(buf.readInt8() == 'H');
    CHECK(buf.readInt16() == 'T' * 256 + 'T');
    CHECK(buf.readInt8() == 'P');
    CHECK(buf.readableBytes() == 0);
    CHECK(buf.writableBytes() == Buffer::kInitialSize);

    // 负数往返（有符号类型直接按网络字节序存取）
    buf.appendInt8(-1);
    buf.appendInt16(-2);
    buf.appendInt32(-3);
    CHECK(buf.readableBytes() == 7);
    CHECK(buf.readInt8() == -1);
    CHECK(buf.readInt16() == -2);
    CHECK(buf.readInt32() == -3);
}

static void testBufferReadInt64Boundary() {
    Buffer buf;

    buf.appendInt64(INT64_MAX);
    buf.appendInt64(INT64_MIN);
    buf.appendInt64(-1);
    buf.appendInt64(0);
    buf.appendInt64(1234567890123LL);
    CHECK(buf.readableBytes() == 5 * sizeof(int64_t));

    CHECK(buf.readInt64() == INT64_MAX);
    CHECK(buf.readInt64() == INT64_MIN);
    CHECK(buf.readInt64() == -1);
    CHECK(buf.readInt64() == 0);
    CHECK(buf.readInt64() == 1234567890123LL);
    CHECK(buf.readableBytes() == 0);

    // peekInt64 不消耗
    buf.appendInt64(0x0102030405060708LL);
    CHECK(buf.peekInt64() == 0x0102030405060708LL);
    CHECK(buf.readableBytes() == sizeof(int64_t));
    buf.retrieve(sizeof(int64_t));  // 消耗掉后再追加新数据

    // 边界值同样适用于 int32/int16
    buf.appendInt32(INT32_MIN);
    buf.appendInt32(INT32_MAX);
    CHECK(buf.readInt32() == INT32_MIN);
    CHECK(buf.readInt32() == INT32_MAX);
    buf.appendInt16(INT16_MIN);
    buf.appendInt16(INT16_MAX);
    CHECK(buf.readInt16() == INT16_MIN);
    CHECK(buf.readInt16() == INT16_MAX);
    buf.appendInt8(INT8_MIN);
    buf.appendInt8(INT8_MAX);
    CHECK(buf.readInt8() == INT8_MIN);
    CHECK(buf.readInt8() == INT8_MAX);
    CHECK(buf.readableBytes() == 0);
}

static void testBufferPrependInt() {
    // prepend 系列写入 prepend 区（网络字节序）；prepend 后的数据在
    // 可读区最前面（栈式：最近 prepend 的最先被 readInt* 读到）
    Buffer buf;
    buf.append("hello", 5);
    buf.prependInt16(-2);
    buf.prependInt8(-1);
    buf.prependInt32(-3);
    CHECK(buf.readableBytes() == 5 + 7);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend - 7);

    // 按栈顺序读出：-3, -1, -2，然后是原来的 "hello"
    CHECK(buf.readInt32() == -3);
    CHECK(buf.readInt8() == -1);
    CHECK(buf.readInt16() == -2);
    CHECK(buf.retrieveAllAsString() == "hello");

    // peekPrependInt* 读取 prepend 区（网络字节序，最近 prepend 的在前）
    Buffer b2;
    b2.append("abc", 3);
    b2.prependInt32(7);
    b2.prependInt32(8);
    CHECK(b2.peekPrependInt32() == 8);  // 最近 prepend 的在最前面
    CHECK(b2.readInt32() == 8);
    CHECK(b2.readInt32() == 7);
    CHECK(b2.retrieveAllAsString() == "abc");

    Buffer b3;
    b3.append("xy", 2);
    b3.prependInt16(100);
    b3.prependInt16(200);
    CHECK(b3.peekPrependInt16() == 200);
    CHECK(b3.readInt16() == 200);
    CHECK(b3.readInt16() == 100);
    CHECK(b3.retrieveAllAsString() == "xy");

    Buffer b4;
    b4.append("z", 1);
    b4.prependInt8(-1);
    b4.prependInt8(-2);
    CHECK(b4.peekPrependInt8() == -2);
    CHECK(b4.readInt8() == -2);
    CHECK(b4.readInt8() == -1);
    CHECK(b4.retrieveAllAsString() == "z");
}

static void testBufferFindEOL() {
    Buffer buf;
    buf.append(string(100000, 'x').data(), 100000);
    CHECK(buf.findEOL() == NULL);
    CHECK(buf.findEOL(buf.peek() + 90000) == NULL);
    CHECK(buf.findCRLF() == NULL);

    // 找到 '\n' 的场景
    Buffer buf2;
    buf2.append("abc\ndef", 7);
    CHECK(buf2.findEOL() == buf2.peek() + 3);
    CHECK(buf2.findCRLF() == NULL);
    buf2.retrieveUntil(buf2.findEOL() + 1);
    CHECK(buf2.readableBytes() == 3);

    // 找到 CRLF 的场景
    Buffer buf3;
    buf3.append("abc\r\nxyz", 8);
    CHECK(buf3.findCRLF() == buf3.peek() + 3);
    CHECK(buf3.findEOL(buf3.peek() + 2) == buf3.peek() + 4);
}

static void testBufferRetrieve() {
    Buffer buf;
    buf.append(string(10, 'a').data(), 10);
    buf.retrieve(4);  // 部分取出
    CHECK(buf.readableBytes() == 6);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend + 4);

    buf.retrieve(100);  // 超过可读长度 → 等价 retrieveAll
    CHECK(buf.readableBytes() == 0);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend);
    CHECK(buf.empty());

    // retrieveUntil
    buf.append("1234567890", 10);
    buf.retrieveUntil(buf.peek() + 5);
    CHECK(buf.readableBytes() == 5);
    CHECK(buf.retrieveAllAsString() == "67890");
    CHECK(buf.empty());
}

static void testBufferSwap() {
    Buffer a, b;
    a.append("aaa", 3);
    b.append("bb", 2);
    jpmuduo::swap(a, b);
    CHECK(a.retrieveAllAsString() == "bb");
    CHECK(b.retrieveAllAsString() == "aaa");
}

static void output(Buffer&& buf, const void* inner) {
    Buffer newbuf(std::move(buf));
    // printf("New Buffer at %p, inner %p\n", &newbuf, newbuf.peek());
    CHECK(inner == newbuf.peek());
}

// 注意：原版注释“g++ 4.4 失败，4.6 通过”，这里用 g++ 现代版本验证移动构造
static void testBufferMove() {
    Buffer buf;
    buf.append("muduo", 5);
    const void* inner = buf.peek();
    // printf("Buffer at %p, inner %p\n", &buf, inner);
    output(std::move(buf), inner);

    // 移动后的源对象应回到初始状态
    CHECK(buf.readableBytes() == 0);
    CHECK(buf.prependableBytes() == Buffer::kCheapPrepend);

    // 移动赋值
    Buffer dst;
    dst.append("dst", 3);
    dst = std::move(buf);
    CHECK(dst.readableBytes() == 0);
}

int main() {
    testBufferAppendRetrieve();
    testBufferGrow();
    testBufferInsideGrow();
    testBufferShrink();
    testBufferPrepend();
    testBufferReadInt();
    testBufferReadInt64Boundary();
    testBufferPrependInt();
    testBufferFindEOL();
    testBufferRetrieve();
    testBufferSwap();
    testBufferMove();

    if (g_failures > 0) {
        fprintf(stderr, "Buffer_unittest: %d check(s) failed\n", g_failures);
        return 1;
    }
    printf("Buffer_unittest: all checks passed\n");
    return 0;
}
