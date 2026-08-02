//
// Created by jianp on 2025/12/11.
//

#ifndef JPMUDUO_CALLBACKS_H
#define JPMUDUO_CALLBACKS_H

#include <memory>
#include <functional>

namespace jpmuduo {

class Buffer;
class TcpConnection;
class TimeStamp;
class InetAddress;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*, TimeStamp)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;

// UDP callbacks
using UdpServerMessageCallback = std::function<void(Buffer*, TimeStamp, const InetAddress& senderAddr)>;

}  // namespace jpmuduo

#endif //JPMUDUO_CALLBACKS_H
