#include <doctest/doctest.h>

#include <memory>
#include <type_traits>
#include <utility>

#include "pjh_result/option.hpp"
#include "support/instrumented.hpp"

namespace res = pjh::result;
using pjh_test::InstanceCounter;

namespace
{
    bool is_positive(int &v)
    {
        return v > 0;
    }

    template <typename T>
    concept RvalueTakeIf = requires(T t) {
        std::move(t).take_if([](int &v) { return v > 0; });
    };

    template <typename T>
    concept ConstTakeIf = requires(const T &t) {
        t.take_if([](int &v) { return v > 0; });
    };

    template <typename T>
    concept RvalueInsert = requires(T t) { std::move(t).insert(1); };

    template <typename T>
    concept RvalueGetOrInsert = requires(T t) { std::move(t).get_or_insert(1); };

    template <typename T>
    concept RvalueGetOrInsertDefault = requires(T t) { std::move(t).get_or_insert_default(); };

    template <typename T>
    concept RvalueGetOrInsertWith = requires(T t) { std::move(t).get_or_insert_with([] { return 1; }); };

    template <typename T>
    concept RvalueFilterMutable = requires(T t) {
        std::move(t).filter([](int &v) { return v > 0; });
    };

    template <typename T>
    concept ConstFilterMutable = requires(const T &t) {
        t.filter([](int &v) { return v > 0; });
    };

    template <typename T>
    concept RvalueFilterConstRef = requires(T t) {
        std::move(t).filter([](const int &v) { return v > 0; });
    };

    template <typename T>
    concept RvalueFilterNullary = requires(T t) {
        std::move(t).filter([] { return true; });
    };
}

// 编译期：take_if 返回 Option<T>，且只能作用于非 const 左值
static_assert(std::is_same_v<
              decltype(std::declval<res::Option<int> &>().take_if(&is_positive)),
              res::Option<int>>);
static_assert(!RvalueTakeIf<res::Option<int>>);
static_assert(!ConstTakeIf<res::Option<int>>);
static_assert(requires(res::Option<int> &o) { o.take_if(&is_positive); });

// 编译期：in-place 引用返回的 mutator 只能作用于非 const 左值，右值调用被拒绝
static_assert(!RvalueInsert<res::Option<int>>);
static_assert(!RvalueGetOrInsert<res::Option<int>>);
static_assert(!RvalueGetOrInsertDefault<res::Option<int>>);
static_assert(!RvalueGetOrInsertWith<res::Option<int>>);
static_assert(requires(res::Option<int> &o) {
    o.insert(1);
    o.get_or_insert(2);
    o.get_or_insert_default();
    o.get_or_insert_with([] { return 3; });
});
static_assert(std::is_same_v<decltype(std::declval<res::Option<int> &>().insert(1)), int &>);

// 编译期：filter()&& 体传可变左值，接受 F(T&)；const& 体传 const T&，仍拒绝 F(T&)
static_assert(RvalueFilterMutable<res::Option<int>>);
static_assert(!ConstFilterMutable<res::Option<int>>);
static_assert(RvalueFilterConstRef<res::Option<int>>);
static_assert(RvalueFilterNullary<res::Option<void>>);

TEST_CASE("take moves the value out and leaves None")
{
    auto o = res::Option<int>::Some(5);
    auto taken = o.take();
    CHECK(taken.unwrap() == 5);
    CHECK(o.is_none());

    auto empty = res::Option<int>::None();
    CHECK(empty.take().is_none());
}

TEST_CASE("replace swaps the value and returns the old one")
{
    auto o = res::Option<int>::Some(1);
    auto old = o.replace(2);
    CHECK(old.unwrap() == 1);
    CHECK(o.unwrap() == 2);

    auto n = res::Option<int>::None();
    auto prev = n.replace(9);
    CHECK(prev.is_none());
    CHECK(n.unwrap() == 9);
}

TEST_CASE("insert overwrites and returns a reference")
{
    auto o = res::Option<int>::Some(1);
    int &r = o.insert(42);
    CHECK(r == 42);
    CHECK(o.unwrap() == 42);
    r = 43;
    CHECK(o.unwrap() == 43);
}

TEST_CASE("get_or_insert only inserts when None")
{
    auto none = res::Option<int>::None();
    CHECK(none.get_or_insert(7) == 7);
    CHECK(none.unwrap() == 7);

    auto some = res::Option<int>::Some(1);
    CHECK(some.get_or_insert(99) == 1); // already Some, keep
    CHECK(some.unwrap() == 1);
}

TEST_CASE("get_or_insert_default default-constructs when None")
{
    auto none = res::Option<int>::None();
    int &r = none.get_or_insert_default();
    CHECK(r == 0);
    CHECK(none.unwrap() == 0);
    r = 42;
    CHECK(none.unwrap() == 42);

    auto some = res::Option<int>::Some(7);
    CHECK(some.get_or_insert_default() == 7);
}

TEST_CASE("get_or_insert_with calls f when None")
{
    auto none = res::Option<int>::None();
    CHECK(none.get_or_insert_with([] { return 99; }) == 99);
    CHECK(none.unwrap() == 99);

    auto some = res::Option<int>::Some(5);
    bool called = false;
    CHECK(some.get_or_insert_with([&] { called = true; return -1; }) == 5);
    CHECK(!called);
}

TEST_CASE("filter rvalue accepts a mutable-lvalue predicate")
{
    auto o = res::Option<int>::Some(41);
    auto kept = std::move(o).filter(
        [](int &v)
        {
            ++v;             // 41 -> 42
            return v == 42;
        });
    CHECK(kept.is_some());
    CHECK(kept.unwrap() == 42);

    auto dropped_src = res::Option<int>::Some(41);
    auto dropped = std::move(dropped_src).filter(
        [](int &v)
        {
            ++v;             // 41 -> 42
            return v == 99;
        });
    CHECK(dropped.is_none());
}

TEST_CASE("mutations do not leak")
{
    InstanceCounter::reset();
    {
        auto o = res::Option<InstanceCounter>::Some(InstanceCounter{1});
        auto old = o.replace(InstanceCounter{2});
        o.insert(InstanceCounter{3});
        auto taken = o.take();
        CHECK(taken.unwrap().id == 3);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("take works on void option")
{
    auto o = res::Option<void>::Some();
    auto taken = o.take();
    CHECK(taken.is_some());
    CHECK(o.is_none());
}

TEST_CASE("take_if takes the value when the predicate holds")
{
    auto o = res::Option<int>::Some(42);
    auto taken = o.take_if([](int &v) { return v == 42; });
    CHECK(taken.is_some());
    CHECK(taken.unwrap() == 42);
    CHECK(o.is_none());
}

TEST_CASE("take_if keeps the value when the predicate fails")
{
    auto o = res::Option<int>::Some(42);
    auto taken = o.take_if([](int &) { return false; });
    CHECK(taken.is_none());
    CHECK(o.is_some());
    CHECK(o.unwrap() == 42);
}

TEST_CASE("take_if predicate may mutate the value even when it returns false")
{
    auto o = res::Option<int>::Some(42);
    auto taken = o.take_if(
        [](int &v)
        {
            ++v;             // 42 -> 43
            return v == 42;  // false: value stays (mutated) in the source
        });
    CHECK(taken.is_none());
    CHECK(o.is_some());
    CHECK(o.unwrap() == 43);
}

TEST_CASE("take_if takes the mutated value when the predicate then holds")
{
    auto o = res::Option<int>::Some(41);
    auto taken = o.take_if(
        [](int &v)
        {
            ++v;             // 41 -> 42
            return v == 42;  // true: value removed
        });
    CHECK(taken.is_some());
    CHECK(taken.unwrap() == 42);
    CHECK(o.is_none());
}

TEST_CASE("take_if does not invoke the predicate on None")
{
    auto o = res::Option<int>::None();
    int calls = 0;
    auto taken = o.take_if(
        [&](int &)
        {
            ++calls;
            return true;
        });
    CHECK(taken.is_none());
    CHECK(calls == 0);
    CHECK(o.is_none());
}

TEST_CASE("take_if works with a move-only payload")
{
    auto o = res::Option<std::unique_ptr<int>>::Some(std::unique_ptr<int>(new int(5)));
    auto taken = o.take_if([](std::unique_ptr<int> &p) { return *p == 5; });
    CHECK(taken.is_some());
    CHECK(*taken.unwrap() == 5);
    CHECK(o.is_none());

    auto kept = res::Option<std::unique_ptr<int>>::Some(std::unique_ptr<int>(new int(6)));
    auto not_taken = kept.take_if([](std::unique_ptr<int> &p) { return *p == 99; });
    CHECK(not_taken.is_none());
    CHECK(kept.is_some());
    CHECK(*kept.unwrap() == 6);
}

TEST_CASE("take_if works on void option")
{
    auto o = res::Option<void>::Some();
    auto taken = o.take_if([] { return true; });
    CHECK(taken.is_some());
    CHECK(o.is_none());

    auto kept = res::Option<void>::Some();
    CHECK(kept.take_if([] { return false; }).is_none());
    CHECK(kept.is_some());
}

TEST_CASE("take_if does not leak in either branch")
{
    InstanceCounter::reset();
    {
        auto taken_src = res::Option<InstanceCounter>::Some(InstanceCounter{1});
        auto taken = taken_src.take_if([](InstanceCounter &c) { return c.id == 1; });
        CHECK(taken.unwrap().id == 1);

        auto kept_src = res::Option<InstanceCounter>::Some(InstanceCounter{2});
        auto not_taken = kept_src.take_if([](InstanceCounter &c) { return c.id == 99; });
        CHECK(not_taken.is_none());
        CHECK(kept_src.unwrap().id == 2);
    }
    CHECK(InstanceCounter::live == 0);
}
