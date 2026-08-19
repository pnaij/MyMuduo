// 对照原始 muduo TimerId.h 改写：
//   - namespace muduo::net -> jpmuduo
//   - 从 Timer.h 中抽出独立文件：TimerId 是不透明标识符，只用于
//     取消定时器（TimerQueue::cancel），对用户隐藏 Timer 指针
//   - jpmuduo 版保留 timer()/sequence() 访问器（原始 muduo 只向
//     TimerQueue 开放，此处为方便测试保留）

#ifndef JPMUDUO_NET_TIMERID_H
#define JPMUDUO_NET_TIMERID_H

#include <stdint.h>

namespace jpmuduo {

class Timer;

///
/// An opaque identifier, for canceling Timer.
///
class TimerId {
public:
    TimerId()
        : timer_(nullptr)
        , sequence_(0)
    {
    }

    TimerId(Timer* timer, int64_t seq)
        : timer_(timer)
        , sequence_(seq)
    {
    }

    // default copy-ctor, dtor and assignment are okay

    Timer* timer() const { return timer_; }
    int64_t sequence() const { return sequence_; }

private:
    Timer* timer_;
    int64_t sequence_;
};

}  // namespace jpmuduo

#endif  // JPMUDUO_NET_TIMERID_H
