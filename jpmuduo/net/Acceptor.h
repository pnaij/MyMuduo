//
// Created by jianp on 2025/12/10.
//

#ifndef JPMUDUO_ACCEPTOR_H
#define JPMUDUO_ACCEPTOR_H

#include "jpmuduo/base/noncopyable.h"
#include "jpmuduo/net/Socket.h"
#include "jpmuduo/net/Channel.h"

#include <functional>

namespace jpmuduo {

class EventLoop;
class InetAddress;

class Acceptor : noncopyable {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb) {
        newConnectionCallback_ = cb;
    }

    bool listening() const { return listening_; }
    void listen();
private:
    void handleRead();

    EventLoop *loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    int idleFd_;

    bool listening_;
};


}  // namespace jpmuduo

#endif //JPMUDUO_ACCEPTOR_H
