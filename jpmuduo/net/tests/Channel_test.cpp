// Channel_test.cpp
//
// 仿照 muduo/muduo/net/tests/Channel_test.cc 的思路重写（自包含 main + CHECK）。
// 原版依赖 muduo::net::detail 里的 free 函数 createTimerfd()/readTimerfd()；
// jpmuduo 的 TimerQueue 把这些实现为私有成员，测试无法复用，因此本测试
// 直接在测试代码里 timerfd_create / socketpair 构造事件源，验证：
//   - enableReading 后事件回调被触发（timerfd、socketpair 两种 fd）
//   - 事件回调里 disableAll 后不再触发
//   - remove() 生命周期：disableAll + remove 后 Channel 可安全析构
//   - enableWriting/disableWriting 与可写事件回调
//   - POLLHUP / POLLERR / POLLNVAL 等 revents 的派发逻辑（直接调 handleEvent）
//   - tie() 保护
//
// Channel 公开接口：handleEvent / enableReading / disableAll / remove /
// setReadCallback / setWriteCallback / setCloseCallback / setErrorCallback /
// tie / doNotLogHup / isNoneEvent / isReading / isWriting 等。

#include "jpmuduo/net/Channel.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/base/TimeStamp.h"

#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <memory>

using jpmuduo::Channel;
using jpmuduo::EventLoop;
using jpmuduo::TimeStamp;

static int g_failures = 0;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #expr);                                        \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static void runLoopUntil(EventLoop& loop, const bool* condition) {
    // 每 20ms 检查一次条件，最多 2 秒
    loop.runEvery(0.02, [&] {
        if (*condition) {
            loop.quit();
        }
    });
    loop.runAfter(2.0, [&] {
        printf("watchdog: condition not met, quit anyway\n");
        loop.quit();
    });
    loop.loop();
}

static void testTimerfdChannel() {
    printf("--- testTimerfdChannel ---\n");
    EventLoop loop;

    const int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    CHECK(timerfd >= 0);

    Channel chan(&loop, timerfd);
    bool readFired = false;
    chan.setReadCallback([&](TimeStamp receiveTime) {
        printf("timerfd read callback, receiveTime valid = %d\n",
               receiveTime.valid());
        readFired = true;
        uint64_t expirations = 0;
        (void)::read(timerfd, &expirations, sizeof(expirations));  // 排空
        chan.disableAll();  // 取消关注
    });
    chan.enableReading();
    CHECK(chan.isReading());
    CHECK(!chan.isNoneEvent());
    printf("events: %s\n", chan.eventsToString().c_str());

    struct itimerspec spec;
    memset(&spec, 0, sizeof(spec));
    spec.it_value.tv_nsec = 50 * 1000 * 1000;  // 50ms 后到期
    CHECK(::timerfd_settime(timerfd, 0, &spec, nullptr) == 0);

    runLoopUntil(loop, &readFired);
    CHECK(readFired);
    CHECK(chan.isNoneEvent());  // disableAll 后无关注事件

    // disableAll + remove 后安全析构
    chan.remove();
    ::close(timerfd);
    printf("timerfd channel ok\n");
}

static void testSocketpairChannel() {
    printf("--- testSocketpairChannel ---\n");
    EventLoop loop;

    int sv[2];
    CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    Channel chan(&loop, sv[0]);
    bool readFired = false;
    bool eofSeen = false;
    bool errorFired = false;
    int readCount = 0;

    chan.setReadCallback([&](TimeStamp) {
        ++readCount;
        char c = 0;
        const ssize_t n = ::read(sv[0], &c, 1);
        if (n == 1) {
            CHECK(c == 'x');
            readFired = true;
        } else if (n == 0) {
            // 对端关闭：EOF 也会以读事件形式唤醒（POLLIN|POLLHUP），
            // 但 closeCallback 的触发条件是 POLLHUP 且无 POLLIN ——
            // 对端关闭后 epoll 会持续报告 POLLIN（EOF 状态），因此
            // closeCallback 不会因对端关闭而触发（与原版 muduo 行为
            // 一致，原版 Channel_test 也只测 timerfd，不测 HUP）。
            eofSeen = true;
            chan.disableAll();
            loop.quit();
        }
    });
    chan.setErrorCallback([&] { errorFired = true; });
    chan.doNotLogHup();  // 关闭 HUP 日志噪音
    chan.enableReading();

    // 1) 对端写入 → 读事件
    CHECK(::write(sv[1], "x", 1) == 1);
    runLoopUntil(loop, &readFired);
    CHECK(readFired);
    CHECK(readCount >= 1);

    // 2) 对端关闭 → 读回调被 EOF 唤醒（read 返回 0）
    ::close(sv[1]);
    runLoopUntil(loop, &eofSeen);
    CHECK(eofSeen);
    CHECK(!errorFired);

    // 3) 生命周期收尾
    chan.disableAll();
    chan.remove();
    ::close(sv[0]);
    printf("socketpair channel ok (readCount=%d)\n", readCount);
}

static void testWriteCallback() {
    printf("--- testWriteCallback ---\n");
    EventLoop loop;

    int sv[2];
    CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    Channel chan(&loop, sv[0]);
    bool writeFired = false;
    chan.setWriteCallback([&] { writeFired = true; });
    chan.enableWriting();  // 空 socketpair 写缓冲区 → POLLOUT 立即可用
    CHECK(chan.isWriting());

    runLoopUntil(loop, &writeFired);
    CHECK(writeFired);

    chan.disableWriting();
    CHECK(!chan.isWriting());
    chan.disableAll();
    chan.remove();
    ::close(sv[0]);
    ::close(sv[1]);
    printf("write callback ok\n");
}

static void testManualReventsDispatch() {
    printf("--- testManualReventsDispatch ---\n");
    // 不经过 poller，直接设置 revents 后调用 handleEvent，验证派发逻辑
    EventLoop loop;
    Channel chan(&loop, 1234);

    bool readFired = false;
    bool writeFired = false;
    bool closeFired = false;
    bool errorFired = false;
    chan.setReadCallback([&](TimeStamp) { readFired = true; });
    chan.setWriteCallback([&] { writeFired = true; });
    chan.setCloseCallback([&] { closeFired = true; });
    chan.setErrorCallback([&] { errorFired = true; });

    const TimeStamp now = TimeStamp::now();

    chan.set_revents(POLLIN);
    chan.handleEvent(now);
    CHECK(readFired && !writeFired && !closeFired && !errorFired);

    readFired = writeFired = closeFired = errorFired = false;
    chan.set_revents(POLLOUT);
    chan.handleEvent(now);
    CHECK(writeFired && !readFired && !closeFired && !errorFired);

    // POLLHUP 且无 POLLIN → closeCallback（不触发读）
    readFired = writeFired = closeFired = errorFired = false;
    chan.set_revents(POLLHUP);
    chan.handleEvent(now);
    CHECK(closeFired && !readFired && !writeFired && !errorFired);

    // POLLHUP | POLLIN → 读优先，不触发 close
    readFired = writeFired = closeFired = errorFired = false;
    chan.set_revents(POLLHUP | POLLIN);
    chan.handleEvent(now);
    CHECK(readFired && !closeFired && !writeFired && !errorFired);

    // POLLERR → errorCallback；POLLNVAL → errorCallback
    readFired = writeFired = closeFired = errorFired = false;
    chan.set_revents(POLLERR);
    chan.handleEvent(now);
    CHECK(errorFired && !readFired && !closeFired);

    readFired = writeFired = closeFired = errorFired = false;
    chan.set_revents(POLLNVAL);
    chan.handleEvent(now);
    CHECK(errorFired && !readFired && !closeFired);

    printf("manual revents dispatch ok\n");
}

static void testTie() {
    printf("--- testTie ---\n");
    EventLoop loop;
    const int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    CHECK(timerfd >= 0);

    Channel chan(&loop, timerfd);
    auto guard = std::make_shared<int>(42);
    chan.tie(guard);  // 与对象绑定，对象存活期间事件正常派发

    bool readFired = false;
    chan.setReadCallback([&](TimeStamp) {
        readFired = true;
        uint64_t expirations = 0;
        (void)::read(timerfd, &expirations, sizeof(expirations));
        chan.disableAll();
    });
    chan.enableReading();

    struct itimerspec spec;
    memset(&spec, 0, sizeof(spec));
    spec.it_value.tv_nsec = 30 * 1000 * 1000;  // 30ms
    CHECK(::timerfd_settime(timerfd, 0, &spec, nullptr) == 0);

    runLoopUntil(loop, &readFired);
    CHECK(readFired);  // tie 对象存活 → 回调正常触发

    chan.remove();
    ::close(timerfd);
    printf("tie ok\n");
}

int main() {
    testTimerfdChannel();
    testSocketpairChannel();
    testWriteCallback();
    testManualReventsDispatch();
    testTie();

    if (g_failures > 0) {
        fprintf(stderr, "Channel_test: %d check(s) failed\n", g_failures);
        return 1;
    }
    printf("Channel_test: all checks passed\n");
    return 0;
}
