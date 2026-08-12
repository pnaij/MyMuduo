//
// Created by jianp on 2026/8/2.
//

#include "jpmuduo/base/Exception.h"
#include "jpmuduo/base/CurrentThread.h"

#include <utility>  // std::move

namespace jpmuduo
{

Exception::Exception(std::string msg)
    : message_(std::move(msg)),
      stack_(CurrentThread::stackTrace(/*demangle=*/false))
{
}
}  // namespace jpmuduo