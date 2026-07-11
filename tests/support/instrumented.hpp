#pragma once

#include <stdexcept>
#include <utility>

namespace pjh_test
{
    /// @brief 统计存活实例数，用于验证 Result 无泄漏 / 无 double-free。
    struct InstanceCounter
    {
        static inline int live = 0; ///< 当前存活实例数
        static inline int ctor = 0; ///< 累计构造次数

        int id;

        explicit InstanceCounter(int v = 0) : id(v)
        {
            ++live;
            ++ctor;
        }
        InstanceCounter(const InstanceCounter &o) : id(o.id)
        {
            ++live;
            ++ctor;
        }
        InstanceCounter(InstanceCounter &&o) noexcept : id(o.id)
        {
            o.id = -1;
            ++live;
            ++ctor;
        }
        InstanceCounter &operator=(const InstanceCounter &) = default;
        InstanceCounter &operator=(InstanceCounter &&) noexcept = default;
        ~InstanceCounter() { --live; }

        static void reset()
        {
            live = 0;
            ctor = 0;
        }
    };

    /// @brief 拷贝构造抛异常，move 不抛。用于验证拷贝赋值的强异常保证（Step 1 起）。
    struct ThrowOnCopy
    {
        int id;

        explicit ThrowOnCopy(int v = 0) : id(v) {}
        ThrowOnCopy(const ThrowOnCopy &) { throw std::runtime_error("ThrowOnCopy: copy"); }
        ThrowOnCopy(ThrowOnCopy &&) noexcept = default;
        ThrowOnCopy &operator=(const ThrowOnCopy &) = default;
        ThrowOnCopy &operator=(ThrowOnCopy &&) noexcept = default;
    };

    /// @brief move 构造非 noexcept。用于验证 nothrow-move 约束会拒绝该类型（Step 2 起）。
    struct ThrowingMove
    {
        ThrowingMove() = default;
        ThrowingMove(ThrowingMove &&) {}
        ThrowingMove &operator=(ThrowingMove &&) { return *this; }
    };
}
