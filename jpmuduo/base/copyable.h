//
// Created by jianp on 2026/8/2.
//

#ifndef JPMUDUO_COPYABLE_H
#define JPMUDUO_COPYABLE_H

namespace jpmuduo
{

/// A tag class emphasises the objects are copyable.
/// The empty base class optimization applies.
/// Any derived class of copyable should be a value type.
class copyable
{
protected:
    copyable() = default;
    ~copyable() = default;
};

}  // namespace jpmuduo

#endif  // JPMUDUO_COPYABLE_H