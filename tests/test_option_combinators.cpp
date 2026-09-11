#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "pjh_result/option.hpp"

namespace res = pjh::result;

using IntOpt = res::Option<int>;

namespace
{
    void observe_int(int) {}

    IntOpt make_some(int v)
    {
        return IntOpt::Some(v);
    }

    template <typename T>
    concept HasUnzip = requires(T t) { t.unzip(); };

    std::string join_int_str(int v, const std::string &s)
    {
        return std::to_string(v) + s;
    }

    // Task 16: value-category overloads with distinct return types. Option's const
    // members bind the value as `const T&`, so map_result_t must pick `const int&`.
    struct OverloadedOptionMap
    {
        int operator()(int &&) const;
        long operator()(const int &) const;
    };

    struct LongDefault
    {
        long operator()() const;
    };

    // Task 18: combiner returning void must be rejected by SFINAE.
    struct VoidCombiner
    {
        void operator()(int, int) const;
    };

    struct IntCombiner
    {
        int operator()(int, int) const;
    };

    // Task 35: exposes first_type / second_type and .first / .second members but is not
    // a std::pair, so PairType must reject it instead of letting unzip hard-error.
    struct PairLike
    {
        using first_type = int;
        using second_type = int;
        int first;
        int second;
    };

    template <typename T, typename U, typename F>
    concept LvalueZipWith = requires(const T &a, const U &b, F f)
    {
        a.zip_with(b, f);
    };

    template <typename T, typename U, typename F>
    concept RvalueZipWith = requires(T a, U b, F f)
    {
        std::move(a).zip_with(std::move(b), f);
    };
}

// 编译期：zip_with 拒绝返回 void 的组合子（应无匹配函数，而非函数体硬错）
static_assert(!LvalueZipWith<IntOpt, IntOpt, VoidCombiner>);
static_assert(!RvalueZipWith<IntOpt, IntOpt, VoidCombiner>);
// 编译期：非 void 组合子仍可调用
static_assert(LvalueZipWith<IntOpt, IntOpt, IntCombiner>);
static_assert(RvalueZipWith<IntOpt, IntOpt, IntCombiner>);

// 编译期：跨值类型 zip / zip_with 的返回类型
static_assert(std::is_same_v<
              decltype(std::declval<const IntOpt &>().zip(
                  std::declval<res::Option<std::string>>())),
              res::Option<std::pair<int, std::string>>>);
static_assert(std::is_same_v<
              decltype(std::declval<IntOpt>().zip(
                  std::declval<res::Option<std::string>>())),
              res::Option<std::pair<int, std::string>>>);
static_assert(std::is_same_v<
              decltype(std::declval<const IntOpt &>().zip_with(
                  std::declval<res::Option<std::string>>(), join_int_str)),
              res::Option<std::string>>);
static_assert(std::is_same_v<
              decltype(std::declval<IntOpt>().zip_with(
                  std::declval<res::Option<std::string>>(), join_int_str)),
              res::Option<std::string>>);

// 编译期：Option 的 map/map_or/map_or_else 同样按 `const T&` 推导返回类型
static_assert(std::is_same_v<res::detail::map_result_t<OverloadedOptionMap, int>, long>);
static_assert(std::is_same_v<
              decltype(std::declval<const IntOpt &>().map(
                  std::declval<OverloadedOptionMap>())),
              res::Option<long>>);
static_assert(std::is_same_v<
              decltype(std::declval<const IntOpt &>().map_or(
                  std::declval<long>(), std::declval<OverloadedOptionMap>())),
              long>);
static_assert(std::is_same_v<
              decltype(std::declval<const IntOpt &>().map_or_else(
                  std::declval<LongDefault>(), std::declval<OverloadedOptionMap>())),
              long>);

// 编译期：右值调用按值返回，左值调用仍返回 const 引用
static_assert(std::is_same_v<
              decltype(std::declval<IntOpt>().inspect(&observe_int)),
              IntOpt>);
static_assert(!std::is_reference_v<
              decltype(std::declval<IntOpt>().inspect(&observe_int))>);
static_assert(std::is_same_v<
              decltype(std::declval<IntOpt &>().inspect(&observe_int)),
              const IntOpt &>);

TEST_CASE("map transforms Some, passes None through")
{
    auto s = res::Option<int>::Some(10).map(
        [](int v)
        { return v * 2; });
    CHECK(s.is_some());
    CHECK(s.unwrap() == 20);

    auto n = res::Option<int>::None().map(
        [](int v)
        { return v * 2; });
    CHECK(n.is_none());
}

TEST_CASE("map can change the value type")
{
    auto s = res::Option<int>::Some(3).map(
        [](int v)
        { return std::string(v, 'x'); });
    CHECK(s.unwrap() == "xxx");
}

TEST_CASE("map_or and map_or_else collapse to a value")
{
    CHECK(res::Option<int>::Some(10).map_or(
              -1,
              [](int v)
              { return v * 2; }) == 20);
    CHECK(res::Option<int>::None().map_or(
              -1,
              [](int v)
              { return v * 2; }) == -1);

    CHECK(res::Option<int>::Some(10).map_or_else(
              []()
              { return -1; }, [](int v)
              { return v * 2; }) == 20);
    CHECK(res::Option<int>::None().map_or_else(
              []()
              { return -1; }, [](int v)
              { return v * 2; }) == -1);
}

TEST_CASE("inspect observes Some and returns self")
{
    int seen = 0;
    auto o = res::Option<int>::Some(7);
    const auto &same = o.inspect(
        [&](int v)
        { seen = v; });
    CHECK(seen == 7);
    CHECK(&same == &o);

    seen = 0;
    auto none = res::Option<int>::None();
    none.inspect(
        [&](int v)
        { seen = v; });
    CHECK(seen == 0);
}

TEST_CASE("and_then chains option-returning operations")
{
    auto s = res::Option<int>::Some(3).and_then(
        [](int v)
        { return res::Option<int>::Some(v + 100); });
    CHECK(s.unwrap() == 103);

    auto n = res::Option<int>::None().and_then(
        [](int v)
        { return res::Option<int>::Some(v + 100); });
    CHECK(n.is_none());
}

TEST_CASE("or_else recovers from None")
{
    auto r = res::Option<int>::None().or_else(
        []()
        { return res::Option<int>::Some(0); });
    CHECK(r.unwrap() == 0);

    auto keep = res::Option<int>::Some(7).or_else(
        []()
        { return res::Option<int>::Some(0); });
    CHECK(keep.unwrap() == 7);
}

TEST_CASE("filter keeps or drops the value")
{
    auto kept = res::Option<int>::Some(10).filter(
        [](int v)
        { return v > 5; });
    CHECK(kept.is_some());
    CHECK(kept.unwrap() == 10);

    auto dropped = res::Option<int>::Some(3).filter(
        [](int v)
        { return v > 5; });
    CHECK(dropped.is_none());

    auto none = res::Option<int>::None().filter(
        [](int)
        { return true; });
    CHECK(none.is_none());
}

TEST_CASE("filter rvalue works with move-only type")
{
    auto o = res::Option<std::unique_ptr<int>>::Some(std::unique_ptr<int>(new int(42)));
    auto kept = std::move(o).filter(
        [](const std::unique_ptr<int> &p)
        { return *p > 0; });
    CHECK(kept.is_some());
    CHECK(*kept.unwrap() == 42);

    auto empty = res::Option<std::unique_ptr<int>>::None();
    auto none = std::move(empty).filter(
        [](const std::unique_ptr<int> &)
        { return true; });
    CHECK(none.is_none());
}

TEST_CASE("flatten collapses nested Option")
{
    auto inner = res::Option<int>::Some(42);
    auto outer = res::Option<res::Option<int>>::Some(std::move(inner));
    auto flat = outer.flatten();
    CHECK(flat.is_some());
    CHECK(flat.unwrap() == 42);
}

TEST_CASE("flatten on None returns None")
{
    auto outer = res::Option<res::Option<int>>::None();
    auto flat = outer.flatten();
    CHECK(flat.is_none());
}

TEST_CASE("flatten on Some(None) returns None")
{
    auto outer = res::Option<res::Option<int>>::Some(res::Option<int>::None());
    auto flat = outer.flatten();
    CHECK(flat.is_none());
}

TEST_CASE("flatten rvalue moves inner Option")
{
    auto outer = res::Option<res::Option<std::unique_ptr<int>>>::Some(
        res::Option<std::unique_ptr<int>>::Some(std::unique_ptr<int>(new int(7))));
    auto flat = std::move(outer).flatten();
    CHECK(flat.is_some());
    CHECK(*flat.unwrap() == 7);
}

TEST_CASE("flatten rvalue on None returns None")
{
    auto outer = res::Option<res::Option<int>>::None();
    auto flat = std::move(outer).flatten();
    CHECK(flat.is_none());
}

TEST_CASE("x_or on exactly one Some keeps it")
{
    auto some_a = res::Option<int>::Some(1);
    auto some_b = res::Option<int>::Some(2);
    auto none = res::Option<int>::None();

    CHECK(some_a.x_or(none).unwrap() == 1);
    CHECK(none.x_or(some_a).unwrap() == 1);
    CHECK(some_a.x_or(some_b).is_none());
    CHECK(none.x_or(none).is_none());
}

TEST_CASE("x_or rvalue moves the value")
{
    auto a = res::Option<std::string>::Some("hello");
    auto b = res::Option<std::string>::None();
    auto r = std::move(a).x_or(std::move(b));
    CHECK(r.unwrap() == "hello");
}

TEST_CASE("zip pairs two Somes")
{
    auto a = res::Option<int>::Some(1);
    auto b = res::Option<int>::Some(2);
    auto z = a.zip(b);
    CHECK(z.is_some());
    CHECK(z.unwrap() == std::make_pair(1, 2));
}

TEST_CASE("zip with None returns None")
{
    auto a = res::Option<int>::Some(1);
    auto b = res::Option<int>::None();
    CHECK(a.zip(b).is_none());
    CHECK(res::Option<int>::None().zip(a).is_none());
}

TEST_CASE("zip rvalue moves values")
{
    auto a = res::Option<std::unique_ptr<int>>::Some(std::unique_ptr<int>(new int(7)));
    auto b = res::Option<std::unique_ptr<int>>::Some(std::unique_ptr<int>(new int(8)));
    auto z = std::move(a).zip(std::move(b));
    CHECK(z.is_some());
    CHECK(*z.unwrap().first == 7);
    CHECK(*z.unwrap().second == 8);
}

TEST_CASE("zip_with combines with a function")
{
    auto a = res::Option<int>::Some(3);
    auto b = res::Option<int>::Some(4);
    auto z = a.zip_with(b, [](int x, int y)
                        { return x + y; });
    CHECK(z.is_some());
    CHECK(z.unwrap() == 7);
}

TEST_CASE("zip_with with None returns None")
{
    auto a = res::Option<int>::Some(3);
    auto b = res::Option<int>::None();
    CHECK(a.zip_with(b, [](int x, int y)
                     { return x + y; })
              .is_none());
}

TEST_CASE("zip_with rvalue moves values into combiner")
{
    auto a = res::Option<std::string>::Some(std::string("hello"));
    auto b = res::Option<std::string>::Some(std::string("world"));
    auto z = std::move(a).zip_with(
        std::move(b),
        [](std::string x, std::string y)
        { return x + " " + y; });
    CHECK(z.is_some());
    CHECK(z.unwrap() == "hello world");
}

TEST_CASE("zip supports different value types")
{
    auto a = res::Option<int>::Some(1);
    auto b = res::Option<std::string>::Some(std::string("x"));
    auto none_str = res::Option<std::string>::None();
    auto none_int = res::Option<int>::None();

    auto z = a.zip(b);
    CHECK(z.is_some());
    CHECK(z.unwrap() == std::make_pair(1, std::string("x")));

    CHECK(a.zip(none_str).is_none());
    CHECK(none_int.zip(b).is_none());
    CHECK(none_int.zip(none_str).is_none());
}

TEST_CASE("zip rvalue supports different value types and moves")
{
    auto a = res::Option<int>::Some(2);
    auto b = res::Option<std::string>::Some(std::string("moved"));
    auto z = std::move(a).zip(std::move(b));
    CHECK(z.is_some());
    CHECK(z.unwrap() == std::make_pair(2, std::string("moved")));

    auto c = res::Option<int>::Some(3);
    CHECK(std::move(c).zip(res::Option<std::string>::None()).is_none());
}

TEST_CASE("zip_with supports different value types")
{
    auto a = res::Option<int>::Some(2);
    auto b = res::Option<std::string>::Some(std::string("b"));
    auto none_str = res::Option<std::string>::None();
    auto none_int = res::Option<int>::None();

    auto z = a.zip_with(b, join_int_str);
    CHECK(z.is_some());
    CHECK(z.unwrap() == "2b");

    CHECK(a.zip_with(none_str, join_int_str).is_none());
    CHECK(none_int.zip_with(b, join_int_str).is_none());
}

TEST_CASE("zip_with rvalue supports different value types and moves")
{
    auto a = res::Option<int>::Some(3);
    auto b = res::Option<std::string>::Some(std::string("!"));
    auto z = std::move(a).zip_with(std::move(b), join_int_str);
    CHECK(z.is_some());
    CHECK(z.unwrap() == "3!");

    auto c = res::Option<int>::Some(4);
    CHECK(std::move(c)
              .zip_with(res::Option<std::string>::None(), join_int_str)
              .is_none());
}

TEST_CASE("inspect rvalue observes only Some and returns by value")
{
    int calls = 0;

    auto some = IntOpt::Some(7);
    auto moved_some = std::move(some).inspect(
        [&](int v)
        {
            ++calls;
            CHECK(v == 7);
        });
    CHECK(calls == 1);
    CHECK(moved_some.is_some());
    CHECK(moved_some.unwrap() == 7);
    CHECK(some.is_some()); // source not reset

    auto none = IntOpt::None();
    auto moved_none = std::move(none).inspect(
        [&](int v)
        {
            ++calls;
            (void)v;
        });
    CHECK(calls == 1); // not invoked on None
    CHECK(moved_none.is_none());
}

TEST_CASE("inspect rvalue chains from a temporary Option")
{
    int seen = 0;
    auto o = make_some(4)
                 .inspect(
                     [&](int v)
                     {
                         ++seen;
                         CHECK(v == 4);
                     })
                 .map(
                     [](int v)
                     { return v * 10; });
    CHECK(seen == 1);
    CHECK(o.is_some());
    CHECK(o.unwrap() == 40);
}

TEST_CASE("lvalue inspect keeps returning a reference to self")
{
    auto o = IntOpt::Some(1);
    const auto &same = o.inspect(&observe_int);
    CHECK(&same == &o);
    CHECK(o.is_some());
}

// 编译期：and_with 返回 Option<U>；or_with 保留 T 返回 Option<T>
static_assert(std::is_same_v<
              decltype(std::declval<IntOpt &>().and_with(
                  std::declval<res::Option<long>>())),
              res::Option<long>>);
static_assert(std::is_same_v<
              decltype(std::declval<IntOpt &>().or_with(std::declval<IntOpt>())),
              IntOpt>);
static_assert(std::is_same_v<
              decltype(std::declval<res::Option<void>>().and_with(
                  std::declval<res::Option<int>>())),
              res::Option<int>>);

TEST_CASE("and_with returns other on Some, None otherwise")
{
    CHECK(res::Option<int>::Some(1).and_with(res::Option<int>::Some(2)).unwrap() == 2);
    CHECK(res::Option<int>::Some(1).and_with(res::Option<int>::None()).is_none());
    CHECK(res::Option<int>::None().and_with(res::Option<int>::Some(2)).is_none());
}

TEST_CASE("and_with can change the value type")
{
    auto o = res::Option<int>::Some(1).and_with(
        res::Option<std::string>::Some(std::string("x")));
    CHECK(o.is_some());
    CHECK(o.unwrap() == "x");
}

TEST_CASE("and_with const lvalue copies other and keeps the receiver")
{
    res::Option<int> base = res::Option<int>::Some(1);
    res::Option<int> other = res::Option<int>::Some(2);
    auto r = base.and_with(other);
    CHECK(r.unwrap() == 2);
    CHECK(other.is_some());
}

TEST_CASE("and_with rvalue consumes the receiver and moves other")
{
    auto src = res::Option<int>::Some(1);
    auto r = std::move(src).and_with(
        res::Option<std::unique_ptr<int>>::Some(std::unique_ptr<int>(new int(5))));
    CHECK(*r.unwrap() == 5);
    CHECK(src.is_none()); // receiver consumed
}

TEST_CASE("or_with keeps Some and replaces None")
{
    CHECK(res::Option<int>::Some(1).or_with(res::Option<int>::Some(2)).unwrap() == 1);
    CHECK(res::Option<int>::None().or_with(res::Option<int>::Some(2)).unwrap() == 2);
}

TEST_CASE("or_with const lvalue copies the Some value")
{
    res::Option<int> base = res::Option<int>::Some(1);
    auto r = base.or_with(res::Option<int>::Some(2));
    CHECK(r.unwrap() == 1);
    CHECK(base.is_some());
}

TEST_CASE("or_with rvalue moves the Some value and consumes the receiver")
{
    auto base =
        res::Option<std::unique_ptr<int>>::Some(std::unique_ptr<int>(new int(3)));
    auto r = std::move(base).or_with(res::Option<std::unique_ptr<int>>::None());
    CHECK(*r.unwrap() == 3);
    CHECK(base.is_none());
}

TEST_CASE("Option<void>::and_with and or_with follow presence")
{
    auto a = res::Option<void>::Some().and_with(res::Option<int>::Some(3));
    CHECK(a.unwrap() == 3);
    CHECK(res::Option<void>::None().and_with(res::Option<int>::Some(3)).is_none());

    CHECK(res::Option<void>::Some()
              .or_with(res::Option<void>::None())
              .is_some());
    CHECK(res::Option<void>::None().or_with(res::Option<void>::Some()).is_some());
}

TEST_CASE("and_with and or_with chain with map")
{
    auto r = res::Option<int>::Some(1)
                 .and_with(res::Option<int>::Some(2))
                 .map([](int v)
                      { return v + 10; });
    CHECK(r.unwrap() == 12);

    auto kept = res::Option<int>::Some(1)
                    .or_with(res::Option<int>::Some(0))
                    .map([](int v)
                         { return v * 2; });
    CHECK(kept.unwrap() == 2);
}

// 编译期：unzip 把 Option<pair<A,B>> 拆成 pair<Option<A>, Option<B>>；
// 非 pair 值类型（含 void）不提供该成员。
static_assert(std::is_same_v<
              decltype(std::declval<res::Option<std::pair<int, std::string>> &>()
                           .unzip()),
              std::pair<res::Option<int>, res::Option<std::string>>>);
static_assert(HasUnzip<res::Option<std::pair<int, int>>>);
static_assert(!HasUnzip<res::Option<void>>);
static_assert(!HasUnzip<res::Option<int>>);

// 编译期：PairType 仅识别真正的 std::pair（元素非引用）；引用元素 pair 与非 pair
// 的 pair-like（仅有 first_type/second_type 或非 std::pair）在约束层被拒绝。
static_assert(res::detail::PairType<std::pair<int, std::string>>);
static_assert(res::detail::PairType<std::pair<const int, std::string>>);
static_assert(!res::detail::PairType<std::pair<int &, int &>>);
static_assert(!res::detail::PairType<std::pair<int &, int>>);
static_assert(!res::detail::PairType<PairLike>);
static_assert(!res::detail::PairType<int>);
static_assert(!res::detail::PairType<void>);
static_assert(!HasUnzip<res::Option<PairLike>>);

TEST_CASE("unzip splits Some(pair) into a pair of Options")
{
    auto o = res::Option<std::pair<int, std::string>>::Some(
        std::make_pair(1, std::string("hi")));
    auto [a, b] = o.unzip();
    CHECK(a.unwrap() == 1);
    CHECK(b.unwrap() == "hi");
    CHECK(o.is_some()); // const lvalue leaves the source intact
}

TEST_CASE("unzip of None yields a pair of Nones")
{
    auto o = res::Option<std::pair<int, std::string>>::None();
    auto [a, b] = o.unzip();
    CHECK(a.is_none());
    CHECK(b.is_none());
}

TEST_CASE("unzip rvalue moves non-trivial and move-only elements")
{
    auto o = res::Option<std::pair<std::string, std::unique_ptr<int>>>::Some(
        std::make_pair(std::string("x"), std::unique_ptr<int>(new int(7))));
    auto [a, b] = std::move(o).unzip();
    CHECK(a.unwrap() == "x");
    CHECK(*b.unwrap() == 7);
    CHECK(o.is_none()); // source consumed
}

TEST_CASE("unzip rvalue of None still yields Nones")
{
    auto o = res::Option<std::pair<std::string, std::unique_ptr<int>>>::None();
    auto [a, b] = std::move(o).unzip();
    CHECK(a.is_none());
    CHECK(b.is_none());
}

TEST_CASE("unzip round-trips with zip")
{
    auto a = res::Option<int>::Some(4);
    auto b = res::Option<int>::Some(5);
    auto zipped = a.zip(b);
    auto [ra, rb] = zipped.unzip();
    CHECK(ra.unwrap() == 4);
    CHECK(rb.unwrap() == 5);
}
