// EventLoopThreadPool_unittest.cpp
//
// 仿照 muduo/muduo/net/tests/EventLoopThreadPool_unittest.cc 重写（自包含
// main + CHECK），针对 jpmuduo::EventLoopThreadPool 验证：
//   - setThreadNum(0)：getNextLoop 恒返回 baseLoop，start 时在 baseLoop
//     线程调用 init 回调
//   - setThreadNum(1)：getNextLoop 轮询到同一个 loop 线程
//   - setThreadNum(3)：getNextLoop 三个线程轮询循环
//   - start/stop（析构时 EventLoopThread 负责 quit + join）
//   - getAllLoops / started / name
//
// 适配说明：原版用 ::sleep(3) 等待定时器触发，这里缩短为 usleep(500ms)。

#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/EventLoopThreadPool.h"
#include "jpmuduo/base/CurrentThread.h"

#include <stdio.h>
#include <unistd.h>

#include <functional>

using namespace jpmuduo;
using jpmuduo::EventLoopThreadPool;

static int g_failures = 0;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #expr);                                        \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static void print(EventLoop* p = nullptr) {
    printf("main(): pid = %d, tid = %d, loop = %p\n", getpid(),
           CurrentThread::tid(), static_cast<void*>(p));
}

static void init(EventLoop* p) {
    printf("init(): pid = %d, tid = %d, loop = %p\n", getpid(),
           CurrentThread::tid(), static_cast<void*>(p));
}

int main() {
    print();

    EventLoop loop;
    loop.runAfter(3.0, [&] {  // 看门狗，正常情况约 0.7s 内完成
        printf("watchdog: quit\n");
        loop.quit();
    });

    {
        printf("Single thread %p:\n", static_cast<void*>(&loop));
        EventLoopThreadPool model(&loop, "single");
        model.setThreadNum(0);
        model.start(init);  // numThreads==0 → init(baseLoop) 在本线程调用
        CHECK(model.started());
        CHECK(model.name() == "single");
        CHECK(model.getNextLoop() == &loop);
        CHECK(model.getNextLoop() == &loop);
        CHECK(model.getNextLoop() == &loop);
        CHECK(model.getAllLoops().size() == 1);
        CHECK(model.getAllLoops()[0] == &loop);

        // baseLoop 上 runInLoop 立即执行
        bool ran = false;
        loop.runInLoop([&] { ran = true; });
        CHECK(ran);
    }

    {
        printf("Another thread:\n");
        EventLoopThreadPool model(&loop, "another");
        model.setThreadNum(1);
        model.start(init);
        EventLoop* nextLoop = model.getNextLoop();
        CHECK(nextLoop != &loop);
        CHECK(model.getAllLoops().size() == 1);
        CHECK(model.getAllLoops()[0] == nextLoop);
        nextLoop->runAfter(0.2, std::bind(print, nextLoop));
        CHECK(nextLoop == model.getNextLoop());
        CHECK(nextLoop == model.getNextLoop());
        ::usleep(500 * 1000);  // 原版 ::sleep(3)
    }

    {
        printf("Three threads:\n");
        EventLoopThreadPool model(&loop, "three");
        model.setThreadNum(3);
        model.start(init);
        CHECK(model.getAllLoops().size() == 3);

        EventLoop* nextLoop = model.getNextLoop();
        CHECK(nextLoop != &loop);
        nextLoop->runInLoop(std::bind(print, nextLoop));

        EventLoop* second = model.getNextLoop();
        CHECK(second != &loop);
        CHECK(second != nextLoop);

        EventLoop* third = model.getNextLoop();
        CHECK(third != &loop);
        CHECK(third != nextLoop);
        CHECK(third != second);

        // 三个之后回到第一个（轮询循环）
        CHECK(model.getNextLoop() == nextLoop);
        printf("round-robin cycle ok: %p -> %p -> %p\n",
               static_cast<void*>(nextLoop), static_cast<void*>(second),
               static_cast<void*>(third));
    }

    loop.loop();

    if (g_failures > 0) {
        fprintf(stderr, "EventLoopThreadPool_unittest: %d check(s) failed\n",
                g_failures);
        return 1;
    }
    printf("EventLoopThreadPool_unittest: all checks passed\n");
    return 0;
}
