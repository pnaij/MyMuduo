// 对照原始 muduo Endian.h 改写：
//   - namespace muduo::net::sockets -> jpmuduo::sockets
//   - 提供主机/网络字节序互转的封装（底层 glibc htobe*/be*toh）
//   - 网络字节序统一大端，socket 数据结构（sin_port 等）一律走此层

#ifndef JPMUDUO_NET_ENDIAN_H
#define JPMUDUO_NET_ENDIAN_H

#include <stdint.h>
#include <endian.h>

namespace jpmuduo {
namespace sockets {

inline uint64_t hostToNetwork64(uint64_t host64)
{
  return htobe64(host64);
}

inline uint32_t hostToNetwork32(uint32_t host32)
{
  return htobe32(host32);
}

inline uint16_t hostToNetwork16(uint16_t host16)
{
  return htobe16(host16);
}

inline uint64_t networkToHost64(uint64_t net64)
{
  return be64toh(net64);
}

inline uint32_t networkToHost32(uint32_t net32)
{
  return be32toh(net32);
}

inline uint16_t networkToHost16(uint16_t net16)
{
  return be16toh(net16);
}

}  // namespace sockets
}  // namespace jpmuduo

#endif  // JPMUDUO_NET_ENDIAN_H
