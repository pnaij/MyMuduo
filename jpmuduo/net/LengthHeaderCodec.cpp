//
// LengthHeaderCodec.cpp
//

#include "jpmuduo/net/LengthHeaderCodec.h"
#include "jpmuduo/net/Buffer.h"
#include "jpmuduo/net/TcpConnection.h"

#include <endian.h>
#include <string>

namespace jpmuduo {

void LengthHeaderCodec::onMessage(const TcpConnectionPtr& conn, Buffer* buf) {
    while (buf->readableBytes() >= kHeaderLen) {
        int32_t len = buf->peekInt32();
        if (static_cast<size_t>(len) > buf->readableBytes() - kHeaderLen) {
            break; // incomplete message, wait for more data
        }
        buf->retrieve(kHeaderLen);

        Buffer payload;
        payload.append(buf->peek(), len);
        buf->retrieve(len);

        if (userCallback_) {
            userCallback_(conn, &payload);
        }
    }
}

void LengthHeaderCodec::send(const TcpConnectionPtr& conn, Buffer* buf) {
    int32_t len = static_cast<int32_t>(buf->readableBytes());
    buf->prependInt32(len);
    conn->send(buf);
}

void LengthHeaderCodec::send(const TcpConnectionPtr& conn, const void* data, size_t len) {
    int32_t be = htobe32(static_cast<int32_t>(len));
    std::string message;
    message.append(reinterpret_cast<const char*>(&be), sizeof(be));
    message.append(static_cast<const char*>(data), len);
    conn->send(message);
}

}  // namespace jpmuduo
