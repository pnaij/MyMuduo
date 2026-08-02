# muduoSelf vs 原始 muduo pingpong 吞吐量差异分析

## 测试环境
- WSL2 Linux, GCC 11.4, 本机回环 127.0.0.1:8000
- muduoSelf: `example/benchmark.cpp` + `example/benchserver.cpp`（库实现）
- muduoSelf raw: `example/pingpong.cpp`（裸 epoll，绕过库）
- 原始 muduo: `examples/pingpong/client.cc` + `examples/pingpong/server.cc`
- 消息: 64B, 长度前缀帧协议(4B header + payload)

---

## 问题列表（按影响程度排序）

### 1. [致命] 编译无优化 — CMakeLists.txt 缺少 `-O2`

**位置**: `CMakeLists.txt:4`
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")
```
无 `-O2`/`-O3`。std::copy、memcpy、函数调用、STL 容器等均不会被内联优化。

**修复**: 添加 `-O2 -DNDEBUG`，或使用 CMake 的 `CMAKE_BUILD_TYPE=Release`。

**估算影响**: 2-5x

---

### 2. [致命] 热路径上的 LOG_INFO 开销

**位置**:
- `src/EPollPoller.cpp:32` — 每次 `epoll_wait` 都 LOG_INFO
- `src/EPollPoller.cpp:39` — 每次有事件返回都 LOG_INFO
- `src/EPollPoller.cpp:58` — 每次 updateChannel 都 LOG_INFO
- `src/Channel.cpp:48` — 每个事件处理都 LOG_INFO

`LOG_INFO` 宏每次都执行 `snprintf` + `TimeStamp::now()`，无论输出是否重定向到 /dev/null。

**修复**: 将热路径上的 `LOG_INFO` 改为 `LOG_DEBUG`（只在 `MUDEBUG` 宏定义时生效）。

**估算影响**: 30-50%

---

### 3. [重大] benchserver echo 路径有 3 次数据拷贝

**位置**: `example/benchserver.cpp:52-71`
```cpp
std::string msg = buf->retrieveAsString(len);     // COPY 1: Buffer → string
Buffer out;
out.append(msg.data(), msg.size());                // COPY 2: string → new Buffer
out.prependInt32(...);
conn->send(std::string(out.peek(), out.readableBytes())); // COPY 3: Buffer → string
```

对比原始 muduo (零拷贝):
```cpp
conn->send(buf);
```

**修复**: 给 TcpConnection 添加 `send(Buffer*)` 重载，重写 onMessage 为零拷贝。

**估算影响**: 20-30%

---

### 4. [重大] 缺少 `send(Buffer*)` 零拷贝接口

**位置**: `include/TcpConnection.h:38` — 只有 `send(const std::string& buf)`

原始 muduo 有 `send(Buffer* message)` 和 `send(const StringPiece& message)` 多个重载，允许零拷贝发送。

**修复**: 添加 `send(Buffer* buf)` 和 `send(const void* data, int len)` 重载。

**估算影响**: 10-20%

---

### 5. [中等] benchmark 客户端每条消息多一次 epoll_ctl(MOD)

**位置**: `example/benchmark.cpp:262-265`
```cpp
// 收到响应后切换到 EPOLLOUT
epoll_event ev;
ev.events = EPOLLOUT | EPOLLET;
epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
```

对比 `pingpong.cpp` (裸 epoll): 直接在 EPOLLIN handler 里 write，无需 epoll_ctl。

**修复**: 在 EPOLLIN handler 中直接 write 发送下一个请求，避免额外的 epoll_ctl + epoll_wait 周期。

**估算影响**: 5-10%

---

### 6. [较小] queueInLoop 未使用 std::move

**位置**: `src/EventLoop.cpp:95`
```cpp
pendingFunctors_.emplace_back(cb);  // 拷贝 functor
```

原版 muduo: `pendingFunctors_.push_back(std::move(cb));`

**修复**: 参数改为 `Functor&& cb` 并使用 move。

**估算影响**: <5%

---

### 7. [较小] TcpConnection::send 跨线程时的悬空指针 bug

**位置**: `src/TcpConnection.cpp:66-73`
```cpp
loop_->runInLoop(
    std::bind(&TcpConnection::sendInLoop, this,
              buf.c_str(),   // buf 是 const std::string& 参数
              buf.size())    // 跨线程时可能已析构
);
```

当调用者不在 IO 线程时，`buf` 可能在使用前被析构，导致悬空指针。

**修复**: 使用 `std::string` 拷贝捕获：`std::bind(..., std::string(buf))` 或改用 lambda。

---

## 修复优先级

| 优先级 | 修复项 | 预期收益 |
|--------|--------|----------|
| P0 | CMakeLists.txt 加 `-O2 -DNDEBUG` | 2-5x |
| P0 | 热路径 LOG_INFO → LOG_DEBUG | 30-50% |
| P1 | 添加 `send(Buffer*)` 零拷贝接口 | 20-30% |
| P1 | benchserver onMessage 零拷贝 echo | 20-30% |
| P2 | benchmark 客户端直接在 EPOLLIN 写 | 5-10% |
| P3 | queueInLoop std::move | <5% |

---

## 实测对比 (2026-05-17)

**共用同一 benchserver，仅客户端不同：**

| 测试 | 客户端 | Conns/Threads | 时长 | QPS | 带宽 | 平均延迟 |
|------|--------|--------------|------|-----|------|----------|
| T1 | pingpong (裸 epoll) | 500/2 | 5s | **137,369** | 8.9 MiB/s | N/A |
| T2 | benchmark (muduoSelf 库) | 500/2 | 5s | 103,745 | 6.7 MiB/s | 4.9ms |
| T3 | pingpong (裸 epoll) | 2000/4 | 5s | **138,695** | 9.0 MiB/s | N/A |
| T4 | benchmark (muduoSelf 库) | 2000/4 | 5s | 106,237 | 6.9 MiB/s | 19.4ms |

**关键发现：**
- 裸 epoll 客户端比 muduoSelf 库客户端快 **30-31%**
- 增加连接数/线程数对吞吐提升有限（QPS 几乎不变），瓶颈在**服务端**
- 服务端是同一 benchserver，其中 LOG_INFO 热路径开销 + echo 3 次拷贝是主要瓶颈
- 客户端差异（epoll_ctl(MOD) vs 直接 write）贡献约 30% 的差距

**注意：** 此处没有原始 muduo 库的对比数据（原始 muduo 未经编译）。根据 muduo 作者的 Benchmark 数据，原始 muduo pingpong 在类似条件下通常可达 **200K-500K QPS**（零拷贝 echo + Release 编译）。

---

## 验证方法

1. 裸 epoll 基线: `LD_LIBRARY_PATH=./lib ./bin/pingpong -c 2000 -t 4 -d 10 -s 64`
2. 库实现测试: `LD_LIBRARY_PATH=./lib ./bin/benchmark -c 2000 -t 4 -d 10 -s 64`
3. 原始 muduo pingpong 作为参照基准
