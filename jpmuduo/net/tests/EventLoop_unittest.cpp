// EventLoop_unittest.cpp
//
// 仿照 muduo/muduo/net/tests/EventLoop_unittest.cc 重写（自包含 main + CHECK），
// 针对 jpmuduo::EventLoop 验证 runInLoop / queueInLoop 语义：
//   - loop 线程内 runInLoop 立即执行（不排队）
//   - 跨线程 runInLoop / queueInLoop 唤醒 loop 线程执行
//   - pending functor 按 FIFO 顺序执行
//   - loop 线程内嵌套 runInLoop 在调用点同步执行
//   - loop 线程内 queueInLoop 在下一轮事件循环执行（callingPendingFunctors_ 语义）
//   - quit() 退出 loop
//
// 适配说明（与原始 muduo 的差异）：
//   - 原版 callback() 在 loop 线程内再创建一个 EventLoop；jpmuduo 中同一线程
//     重复创建 EventLoop 会 LOG_FATAL + abort（原版只是 assert，且测试在
//     -DNDEBUG 下不会触发），故此处省略该场景。
//   - 原版 CurrentThread::name() 不存在，打印用 tid()。

#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/base/CurrentThread.h"
#include "jpmuduo/base/Thread.h"

#include <stdio.h>
#include <unistd.h>

#include <atomic>
#include <vector>

using namespace jpmuduo;
using jpmuduo::Thread;

static int g_failures = 0;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #expr);                                        \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

// 在一个独立线程中创建自己的 EventLoop 并跑起来（对应原版 threadFunc）
static void threadFunc() {
    printf("threadFunc(): pid = %d, tid = %d\n", getpid(), CurrentThread::tid());

    CHECK(EventLoop::getEventLoopOfCurrentThread() == nullptr);
    EventLoop loop;
    CHECK(EventLoop::getEventLoopOfCurrentThread() == &loop);
    CHECK(loop.isInLoopThread());
    loop.runAfter(0.1, [&loop] {
        printf("callback in thread loop: tid = %d\n", CurrentThread::tid());
        CHECK(loop.isInLoopThread());
        loop.quit();
    });
    loop.loop();
    printf("threadFunc(): loop exited, tid = %d\n", CurrentThread::tid());
}

// 独立函数：loop 运行中，从另一个线程调用 quit()，应通过 wakeup 唤醒
// 并退出（jpmuduo 与原始 muduo 一样：loop() 开头会重置 quit_，因此
// "quit 先于 loop()" 的用法不受支持 —— 原版注释 FIXME 也承认这一点）。
// 本函数不能在 main 的主 loop 作用域内执行 —— jpmuduo 同一线程创建
// 第二个 EventLoop 会 LOG_FATAL + abort。
static void testQuitFromOtherThread() {
    EventLoop loop2;
    std::thread t([&] {
        usleep(50 * 1000);  // 等 loop() 跑起来
        loop2.quit();
    });
    loop2.loop();  // 约 50ms 后被唤醒退出
    t.join();
    printf("cross-thread quit ok\n");
}

int main() {
    printf("main(): pid = %d, tid = %d\n", getpid(), CurrentThread::tid());

    CHECK(EventLoop::getEventLoopOfCurrentThread() == nullptr);
    // 主场景放进块作用域：主 EventLoop 在块结束时析构，
    // 之后本线程才能创建第二个 EventLoop（见 testQuitThenLoop）
    {
    EventLoop loop;
    CHECK(EventLoop::getEventLoopOfCurrentThread() == &loop);
    CHECK(loop.isInLoopThread());

    // ---- 1. 同线程 runInLoop：立即执行 ----
    bool run1 = false;
    loop.runInLoop([&] { run1 = true; });
    CHECK(run1);

    // ---- 2. 跨线程 runInLoop：排队，由 loop 线程执行 ----
    std::atomic<bool> run2{false};
    std::atomic<pid_t> run2Tid{0};
    {
        std::thread t([&] {
            loop.runInLoop([&] {
                run2 = true;
                run2Tid = CurrentThread::tid();
            });
        });
        t.join();
    }
    // loop 还没开始跑，functor 尚未执行
    CHECK(!run2.load());

    // ---- 3. 跨线程 queueInLoop 多个 functor：FIFO 顺序 ----
    std::vector<int> order;
    {
        std::thread t([&] {
            loop.queueInLoop([&] { order.push_back(1); });
            loop.queueInLoop([&] { order.push_back(2); });
            loop.queueInLoop([&] { order.push_back(3); });
        });
        t.join();
    }

    // ---- 4. loop 线程内嵌套 runInLoop：在调用点同步执行 ----
    // A 是排队执行的 functor；A 内部调用 loop.runInLoop(B)，
    // B 必须在 A 返回前执行完（记录顺序为 1,2,3）
    std::vector<int> seq;
    {
        std::thread t([&] {
            loop.runInLoop([&] {
                seq.push_back(1);  // A start
                loop.runInLoop([&] { seq.push_back(2); });  // B
                seq.push_back(3);  // A end
            });
        });
        t.join();
    }

    // ---- 5. loop 线程内 queueInLoop：下一轮事件循环执行 ----
    // A2 先跑完（aDone = true），期间 queueInLoop(C2)；C2 必须在 A2 之后跑
    std::atomic<bool> aDone{false};
    std::atomic<bool> cRan{false};
    {
        std::thread t([&] {
            loop.queueInLoop([&] {
                aDone = true;
                loop.queueInLoop([&] {
                    CHECK(aDone.load());  // C2 在 A2 完成后才执行
                    cRan = true;
                });
            });
        });
        t.join();
    }

    // ---- 6. 运行 loop：第 2~5 步排队的 functor 全部被执行 ----
    const pid_t mainTid = CurrentThread::tid();
    loop.runAfter(2.0, [&] {
        printf("watchdog: loop not quitted in time, force quit\n");
        loop.quit();
    });
    loop.runAfter(0.5, [&] { loop.quit(); });
    loop.loop();

    // 断言执行结果
    CHECK(run2.load());
    CHECK(run2Tid.load() == mainTid);  // functor 在 loop 线程（即 main）执行
    CHECK(order.size() == 3);
    CHECK(order.size() == 3 && order[0] == 1 && order[1] == 2 && order[2] == 3);
    CHECK(seq.size() == 3);
    CHECK(seq.size() == 3 && seq[0] == 1 && seq[1] == 2 && seq[2] == 3);
    CHECK(cRan.load());
    CHECK(loop.queueSize() == 0);

    // ---- 7. 独立线程跑自己的 EventLoop（对应原版结构） ----
    Thread thread(threadFunc);
    thread.start();
    thread.join();
    }  // 主 EventLoop 在此析构

    CHECK(EventLoop::getEventLoopOfCurrentThread() == nullptr);  // 析构后清空

    if (g_failures > 0) {
        fprintf(stderr, "EventLoop_unittest: %d check(s) failed\n", g_failures);
        return 1;
    }
    // ---- 8. 从别的线程 quit：主 loop 已析构，当前线程可再建 loop ----
    testQuitFromOtherThread();

    if (g_failures > 0) {
        fprintf(stderr, "EventLoop_unittest: %d check(s) failed\n", g_failures);
        return 1;
    }
    printf("EventLoop_unittest: all checks passed\n");
    return 0;
}
