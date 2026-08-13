//
// Created by jianp on 2026/5/16.
//

#include "jpmuduo/net/TcpClient.h"
#include "jpmuduo/net/Connector.h"
#include "jpmuduo/net/TcpConnection.h"
#include "jpmuduo/net/EventLoop.h"
#include "jpmuduo/base/Logging.h"

#include <functional>

namespace jpmuduo {

TcpClient::TcpClient(EventLoop* loop, const InetAddress& serverAddr, const std::string& name)
    : loop_(loop)
    , connector_(new Connector(loop, serverAddr))
    , name_(name)
    , retry_(false)
    , nextConnId_(1)
{
    connector_->setNewConnectionCallback(
        std::bind(&TcpClient::newConnection, this, std::placeholders::_1));
    LOG_INFO << "TcpClient::TcpClient[" << name_ << "]";
}

TcpClient::~TcpClient() {
    LOG_INFO << "TcpClient::~TcpClient[" << name_ << "]";
    connector_->stop();
    disconnect();
}

void TcpClient::connect() {
    LOG_INFO << "TcpClient::connect[" << name_ << "] - connecting to "
             << connector_->serverAddress().toIpPort();
    connector_->start();
}

void TcpClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connection_) {
        connection_->shutdown();
    }
}

void TcpClient::stop() {
    connector_->stop();
}

void TcpClient::newConnection(int sockfd) {
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", connector_->serverAddress().toIpPort().c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    InetAddress localAddr(InetAddress::getLocalAddress(sockfd));
    InetAddress peerAddr(connector_->serverAddress());

    TcpConnectionPtr conn(new TcpConnection(loop_, connName, sockfd, localAddr, peerAddr));
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setHighWaterMarkCallback(highWaterMarkCallback_);
    conn->setCloseCallback(
        std::bind(&TcpClient::removeConnection, this, std::placeholders::_1));

    {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_ = conn;
    }

    conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr& conn) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_.reset();
    }

    loop_->queueInLoop([conn]() {
        conn->connectDestroyed();
    });

    if (retry_) {
        LOG_INFO << "TcpClient::removeConnection[" << name_ << "] - retry connecting";
        connector_->restart();
    }
}

}  // namespace jpmuduo
