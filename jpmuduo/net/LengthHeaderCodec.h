//
// LengthHeaderCodec.h - length-prefixed message framing codec
//

#ifndef JPMUDUO_LENGTHHEADERCODEC_H
#define JPMUDUO_LENGTHHEADERCODEC_H

#include "jpmuduo/base/noncopyable.h"
#include "jpmuduo/net/Callbacks.h"

#include <functional>

namespace jpmuduo {

class Buffer;

class LengthHeaderCodec : noncopyable {
public:
    // User callback receives one complete message per call.
    // The |message| buffer contains exactly the payload bytes (no length header).
    using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer* message)>;

    static constexpr size_t kHeaderLen = sizeof(int32_t);

    explicit LengthHeaderCodec(MessageCallback cb)
        : userCallback_(std::move(cb)) {}

    // Call this from TcpServer/TcpClient's MessageCallback (raw data → framed messages)
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf);

    // Send: prepend length header, then send via connection
    void send(const TcpConnectionPtr& conn, Buffer* buf);
    void send(const TcpConnectionPtr& conn, const void* data, size_t len);

private:
    MessageCallback userCallback_;
};

}  // namespace jpmuduo

#endif // JPMUDUO_LENGTHHEADERCODEC_H
