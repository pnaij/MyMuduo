//
// Created by jianp on 2026/8/2.
//

#include "jpmuduo/base/Exception.h"
#include <utility>  // std::move

namespace jpmuduo
{

Exception::Exception(std::string msg)
    : message_(std::move(msg)),
      stack_()  // TODO: CurrentThread::stackTrace(/*demangle=*/false)
                // 等实现 CurrentThread::stackTrace 后再接入
{
}
}  // namespace jpmuduo