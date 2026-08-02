//
// Created by jianp on 2026/8/2.
//

#ifndef JPMUDUO_EXCEPTION_H
#define JPMUDUO_EXCEPTION_H

#include <exception>
#include <string>

namespace jpmuduo
{

class Exception : public std::exception
{
public:
    Exception(std::string what);
    ~Exception() noexcept override = default;

    // default copy-ctor and operator= are okay.

    const char* what() const noexcept override
    {
        return message_.c_str();
    }

    const char* stackTrace() const noexcept
    {
        return stack_.c_str();
    }

private:
    std::string message_;
    std::string stack_;
};

}  // namespace jpmuduo

#endif  // JPMUDUO_EXCEPTION_H