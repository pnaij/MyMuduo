// EventLoopThread_unittest.cpp
//
// 仿照 muduo/muduo/net/tests/EventLoopThread_unittest.cc 重写（自包含 main + CHECK），
// 针对 jpmuduo::EventLoopThread 验证：
//   - 不 start 直接析构（dtor 安全）
//   - startLoop 返回 loop，runInLoop 跨线程在 loop 线程执行
//   - dtor 内部 quit + join
//   - quit 先于 dtor 调用（loop 线程正常退出后析构）
//   - startLoop 多次调用（见下方说明，用子进程验证）
//
// 适配说明：
//   - 原版用 CurrentThread::sleepUsec，jpmuduo 没有 → 用 usleep()。
//   - 原版 EventLoopThread::startLoop 有 assert(!thread_.started())；
//     jpmuduo 没有该保护，且 Thread::start() 会替换内部 std::thread ——
//     第二次 startLoop 会销毁仍在运行（joinable）的旧 std::thread 导致
//     std::terminate。这是生产代码问题，测试用 fork 的子进程验证并记录，
//     主进程测试继续（详见报告）。

#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/net/EventLoopThread.h"
#include "jpmuduo/base/CurrentThread.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>

using namespace jpmuduo;
using jpmuduo::EventLoopThread;

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
    printf("print: pid = %d, tid = %d, loop = %p\n", getpid(),
           CurrentThread::tid(), static_cast<void*>(p));
}

static void quit(EventLoop* p) {
    print(p);
    p->quit();
}

int main() {
    print();

    {
        // 从未 start，析构应安全
        EventLoopThread thr1;
    }

    {
        // startLoop 后在 loop 线程执行 runInLoop；析构时 dtor 负责 quit + join
        EventLoopThread thr2;
        EventLoop* loop = thr2.startLoop();
        CHECK(loop != nullptr);
        CHECK(loop->isInLoopThread() == false);  // 当前线程不是 loop 线程

        std::atomic<bool> ran{false};
        std::atomic<pid_t> ranTid{0};
        loop->runInLoop([&] {
            ran = true;
            ranTid = CurrentThread::tid();
        });
        for (int i = 0; i < 200 && !ran.load(); ++i) {
            usleep(5000);  // 最多等 1 秒
        }
        CHECK(ran.load());
        CHECK(ranTid.load() != getpid());  // functor 在另一个线程执行
        printf("loop thread tid = %d (main pid = %d)\n", ranTid.load(),
               getpid());

        usleep(500 * 1000);  // 原版: CurrentThread::sleepUsec(500*1000)
        // dtor：quit + join
    }

    {
        // quit() 先于 dtor 调用（对应原版注释 "quit() before dtor"）
        EventLoopThread thr3;
        EventLoop* loop = thr3.startLoop();
        loop->runInLoop(std::bind(quit, loop));
        usleep(500 * 1000);
    }

    {
        // 重复 startLoop 安全性：子进程验证（jpmuduo 的 Thread::start 会
        // 替换 joinable 的 std::thread，预期 abort，见报告）
        pid_t pid = fork();
        if (pid == 0) {
            EventLoopThread thr;
            EventLoop* l1 = thr.startLoop();
            printf("child: first startLoop ok, loop = %p\n",
                   static_cast<void*>(l1));
            EventLoop* l2 = thr.startLoop();  // 第二次调用
            printf("child: second startLoop ok, loop = %p\n",
                   static_cast<void*>(l2));
            if (l2 != nullptr) {
                l2->quit();
            }
            _exit(0);
        }
        CHECK(pid > 0);
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("double startLoop: child exited 0 (ok)\n");
        } else if (WIFSIGNALED(status)) {
            printf("double startLoop: child aborted by signal %d "
                   "(production issue, see report)\n", WTERMSIG(status));
        } else {
            printf("double startLoop: child exited with %d "
                   "(production issue, see report)\n", WEXITSTATUS(status));
        }
    }

    if (g_failures > 0) {
        fprintf(stderr, "EventLoopThread_unittest: %d check(s) failed\n",
                g_failures);
        return 1;
    }
    printf("EventLoopThread_unittest: all checks passed\n");
    return 0;
}
