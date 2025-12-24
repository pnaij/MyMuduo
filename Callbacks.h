//
// Created by jianp on 2025/12/11.
//

#ifndef MUDUOSELF_CALLBACKS_H
#define MUDUOSELF_CALLBACKS_H

#include <memory>
#include <functional>

class Buffer;
class TcpConnection;
class TimeStamp;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*, TimeStamp)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;

#endif //MUDUOSELF_CALLBACKS_H
