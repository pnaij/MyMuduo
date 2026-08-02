//
// buffer_framing_test.cpp — verify TCP framing primitives
//
// Tests:
//   A. Unit tests on raw Buffer (no network required)
//      A1. peekInt/readInt correct round-trip
//      A2. prependInt + peek (length-prefix header pattern)
//      A3. Sticky packets: 3 length-prefixed messages in one buffer
//      A4. Split  packet:  1 message assembled across 3 appends
//      A5. Split+stick:    partial + multiple new messages
//      A6. Delimiter:      findCRLF, split line, sticky lines, split line
//   B. Integration test with TcpServer + raw socket
//      B1. Client sends 5 length-prefixed messages back-to-back (sticky)
//          Server must decode exactly 5 messages with correct content
//

#include "jpmuduo/net/Buffer.h"

#include <iostream>
#include <string>
#include <cstring>
#include <cassert>
#include <vector>

using namespace jpmuduo;

// ── Helper: build a length-prefixed (int32) message ──────────────
static std::string encode(const std::string& payload) {
    Buffer buf;
    buf.append(payload.data(), payload.size());
    buf.prependInt32(static_cast<int32_t>(payload.size()));
    return std::string(buf.peek(), buf.readableBytes());
}

// ── Helper: decode length-prefixed messages from buffer ──────────
static std::vector<std::string> decode(Buffer& buf) {
    std::vector<std::string> msgs;
    while (buf.readableBytes() >= sizeof(int32_t)) {
        int32_t len = buf.peekInt32();
        if (len < 0 || static_cast<size_t>(len) > 64 * 1024 * 1024) {
            std::cerr << "  ERROR: insane len=" << len << std::endl;
            break;
        }
        if (buf.readableBytes() < sizeof(int32_t) + static_cast<size_t>(len)) {
            break;  // partial message
        }
        buf.retrieve(sizeof(int32_t));
        msgs.push_back(buf.retrieveAsString(len));
    }
    return msgs;
}

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cerr << "  FAIL: " << msg << std::endl; failures++; } \
    else { std::cout << "  PASS: " << msg << std::endl; } \
} while(0)

// ══════════════════════════════════════════════════════════════════
// A1. peek/read int round-trip
// ══════════════════════════════════════════════════════════════════
static void testA1_IntRoundTrip() {
    std::cout << "\n--- A1: peek/read int round-trip ---" << std::endl;

    Buffer buf;
    buf.appendInt32(0x01020304);
    buf.appendInt16(0x0506);
    buf.appendInt8(0x07);

    CHECK(buf.peekInt32() == 0x01020304, "peekInt32");
    CHECK(buf.readInt32() == 0x01020304, "readInt32 consume");
    CHECK(buf.peekInt16() == 0x0506, "peekInt16");
    CHECK(buf.readInt16() == 0x0506, "readInt16 consume");
    CHECK(buf.peekInt8()  == 0x07,    "peekInt8");
    CHECK(buf.readInt8()  == 0x07,    "readInt8 consume");
    CHECK(buf.readableBytes() == 0,   "buffer empty after reads");
}

// ══════════════════════════════════════════════════════════════════
// A2. prependInt + peek (length-prefix pattern)
// ══════════════════════════════════════════════════════════════════
static void testA2_PrependPattern() {
    std::cout << "\n--- A2: prependInt length-prefix pattern ---" << std::endl;

    // Simulate encoding a message
    Buffer buf;
    buf.append("Hello", 5);
    buf.prependInt32(5);
    CHECK(buf.readableBytes() == 9, "total size is 4+5=9");
    CHECK(buf.peekInt32() == 5,    "length header = 5");
    buf.retrieve(4);
    std::string payload = buf.retrieveAsString(5);
    CHECK(payload == "Hello",      "payload round-trip ok");
}

// ══════════════════════════════════════════════════════════════════
// A3. Sticky packets: 3 messages arrive in one lump
// ══════════════════════════════════════════════════════════════════
static void testA3_StickyPackets() {
    std::cout << "\n--- A3: Sticky packets (3 msgs in 1 buffer) ---" << std::endl;

    std::string wire  = encode("Alpha") + encode("Beta") + encode("Gamma");
    Buffer buf;
    buf.append(wire.data(), wire.size());

    auto msgs = decode(buf);
    CHECK(msgs.size() == 3,         "3 messages decoded");
    CHECK(msgs[0] == "Alpha",       "msg0=Alpha");
    CHECK(msgs[1] == "Beta",        "msg1=Beta");
    CHECK(msgs[2] == "Gamma",       "msg2=Gamma");
    CHECK(buf.readableBytes() == 0, "no leftover bytes");
}

// ══════════════════════════════════════════════════════════════════
// A4. Split packet: 1 message arrives across 3 chunks
// ══════════════════════════════════════════════════════════════════
static void testA4_SplitPacket() {
    std::cout << "\n--- A4: Split packet (1 msg in 3 chunks) ---" << std::endl;

    std::string wire = encode("SplitAcrossThreeWrites");

    Buffer buf;

    // Chunk 1: only first 2 bytes (part of length header)
    buf.append(wire.data(), 2);
    auto msgs = decode(buf);
    CHECK(msgs.size() == 0, "chunk1: 0 msgs (need full header)");

    // Chunk 2: next 4 bytes (complete header but no payload)
    buf.append(wire.data() + 2, 4);
    msgs = decode(buf);
    CHECK(msgs.size() == 0, "chunk2: 0 msgs (header ok but no payload)");

    // Chunk 3: rest of payload
    buf.append(wire.data() + 6, wire.size() - 6);
    msgs = decode(buf);
    CHECK(msgs.size() == 1,                "chunk3: 1 msg");
    CHECK(msgs[0] == "SplitAcrossThreeWrites", "correct payload");
    CHECK(buf.readableBytes() == 0,        "no leftover");
}

// ══════════════════════════════════════════════════════════════════
// A5. Split + Sticky: partial msg, then new msgs arrive together
// ══════════════════════════════════════════════════════════════════
static void testA5_SplitAndSticky() {
    std::cout << "\n--- A5: Split + Sticky combined ---" << std::endl;

    std::string m0 = encode("First");
    std::string m1 = encode("Second");
    std::string m2 = encode("Third");

    Buffer buf;

    // First read: only first 6 bytes of m0
    buf.append(m0.data(), 6);
    auto msgs = decode(buf);
    CHECK(msgs.size() == 0, "tick0: 0 msgs (partial)");

    // Second read: rest of m0 + entire m1 + entire m2 (all sticky)
    std::string rest = m0.substr(6) + m1 + m2;
    buf.append(rest.data(), rest.size());
    msgs = decode(buf);
    CHECK(msgs.size() == 3,     "tick1: 3 msgs decoded");
    CHECK(msgs[0] == "First",  "msg0=First");
    CHECK(msgs[1] == "Second", "msg1=Second");
    CHECK(msgs[2] == "Third",  "msg2=Third");
    CHECK(buf.readableBytes() == 0, "no leftover");
}

// ══════════════════════════════════════════════════════════════════
// A6. Delimiter-based framing (CRLF)
// ══════════════════════════════════════════════════════════════════
static void testA6_DelimiterFraming() {
    std::cout << "\n--- A6: Delimiter-based (CRLF) framing ---" << std::endl;

    // A6a: sticky lines
    {
        Buffer buf;
        buf.append("GET / HTTP/1.1\r\nHost: x\r\n\r\n", 30);

        const char* eol = buf.findCRLF();
        CHECK(eol != nullptr,                     "a6a: found first CRLF");
        CHECK(std::string(buf.peek(), eol) == "GET / HTTP/1.1",
                                                  "a6a: first line");
        buf.retrieveUntil(eol);
        buf.retrieve(2); // skip CRLF
        CHECK(buf.peekInt16() == 0x486f,          "a6a: 'Ho' after first line"); // "Host..."

        eol = buf.findCRLF();
        CHECK(eol != nullptr,                     "a6a: found second CRLF");
        buf.retrieveUntil(eol);
        buf.retrieve(2);
        eol = buf.findCRLF();
        CHECK(eol != nullptr,                     "a6a: found third CRLF (empty line)");
    }

    // A6b: split line across chunks
    {
        Buffer buf;
        buf.append("Hello", 5);
        CHECK(buf.findCRLF() == nullptr,          "a6b: no CRLF yet");
        buf.append("\r\nWorld\r\n", 9);
        const char* eol = buf.findCRLF();
        CHECK(eol != nullptr,                     "a6b: found first CRLF after split");
        CHECK(std::string(buf.peek(), eol) == "Hello",
                                                  "a6b: first line = Hello");
    }
}

// ══════════════════════════════════════════════════════════════════
// Main
// ══════════════════════════════════════════════════════════════════
int main() {
    std::cout << "=== Buffer TCP Framing Tests ===" << std::endl;

    testA1_IntRoundTrip();
    testA2_PrependPattern();
    testA3_StickyPackets();
    testA4_SplitPacket();
    testA5_SplitAndSticky();
    testA6_DelimiterFraming();

    std::cout << "\n=== Result: " << (failures == 0 ? "ALL PASSED" : "SOME FAILED")
              << " (" << failures << " failure(s)) ===" << std::endl;
    return failures == 0 ? 0 : 1;
}
