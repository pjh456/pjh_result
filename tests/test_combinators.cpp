#include <doctest/doctest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "pjh_result/option.hpp"
#include "pjh_result/result.hpp"

namespace res = pjh::result;
using bad_access = pjh::result::bad_result_access;

using StrResult = res::Result<int, std::string>;

namespace
{
    void observe_ok(int) {}
    void observe_err(const std::string &) {}

    StrResult make_ok(int v)
    {
        return StrResult::Ok(v);
    }

    // Used only to fix the return type in the compile-time assertions below.
    std::string error_from_int(const int &v);
    std::string error_from_void();

    // Task 12: invocable only with an rvalue `int&&`. The const combinators always
    // bind the success value as `const T&`, so the constraint must reject it.
    struct RvalueOnly
    {
        int operator()(int &&) const;
    };

    // Accepted: the const combinators bind the success value as `const T&`.
    struct ConstRefOnly
    {
        int operator()(const int &) const;
    };

    // Task 16: overloaded on value category with distinct return types. The const
    // members bind the success value as `const T&`, so the declared result type
    // must resolve to the `const int&` overload, not the `int&&` one.
    struct OverloadedMap
    {
        int operator()(int &&) const;
        long operator()(const int &) const;
    };

    struct ErrorToLong
    {
        long operator()(const std::string &) const;
    };

    // Task 27: invocable only with an rvalue `std::string&&`. The const error-side
    // combinators bind the error as `const E&`, so the constraint must reject it.
    struct RvalueErrOnly
    {
        long operator()(std::string &&) const;
    };

    // Task 27: overloaded on the error's value category with distinct return types.
    // The declared result type must resolve to the `const std::string&` overload.
    struct OverloadedMapErr
    {
        long operator()(std::string &&) const
        {
            return 0;
        }
        unsigned operator()(const std::string &e) const
        {
            return static_cast<unsigned>(e.size());
        }
    };

    // Task 21: returns a Result and is invocable only with an rvalue `int&&`. The
    // const& overload of and_then must reject it cleanly (SFINAE via the
    // cref_result_t fallback) instead of hard-erroring, while the && overload
    // (value_result_t) accepts it and forwards a moved `int`.
    struct RvalueOnlyResultFn
    {
        res::Result<long, std::string> operator()(int &&v) const
        {
            return res::Result<long, std::string>::Ok(static_cast<long>(v));
        }
    };
}

/// Whether the const-member combinators accept the callable `F` on `R`'s value.
template <typename R, typename F>
concept MapCompat = requires(const R &r, F f) {
    r.map(f);
    r.inspect(f);
    r.is_ok_and(f);
};

/// Whether `and_then` on a const lvalue accepts `F` (the `CrefResultFn` path).
template <typename R, typename F>
concept ConstAndThenCompat = requires(const R &r, F f) {
    { r.and_then(f) };
};

/// Whether `and_then` on an rvalue accepts `F` (the `ValueResultFn` path).
template <typename R, typename F>
concept RvalueAndThenCompat = requires(R &&r, F f) {
    { std::move(r).and_then(f) };
};

/// Whether the const error observers accept the callable `F` (the `const E&` path).
template <typename R, typename F>
concept ErrCompat = requires(const R &r, F f) {
    r.is_err_and(f);
    r.map_err(f);
};

/// Whether `map_or_else` on a const lvalue accepts the error callable `D`.
template <typename R, typename D>
concept MapOrElseCompat = requires(const R &r, D d) {
    r.map_or_else(d, OverloadedMap{});
};

// 编译期：MapCallable 以 `const T&` 判定，rvalue-only 可调用对象应被拒绝
static_assert(!res::detail::MapCallable<RvalueOnly, int>);
static_assert(res::detail::MapCallable<ConstRefOnly, int>);
static_assert(!MapCompat<StrResult, RvalueOnly>);
static_assert(MapCompat<StrResult, ConstRefOnly>);

// 编译期：rvalue-only 且返回 Result 的可调用对象被 const& 版 and_then 干净拒绝
// （SFINAE 而非硬错），&& 版接受；cref_result_t 在不可调用时退化为 void。
static_assert(std::is_same_v<res::detail::cref_result_t<RvalueOnlyResultFn, int>, void>);
// value_result_t 对“无法以 T 调用”的 F 同样退化为 void（&& 版的防御性回退）。
static_assert(std::is_same_v<res::detail::value_result_t<ErrorToLong, int>, void>);
static_assert(!ConstAndThenCompat<StrResult, RvalueOnlyResultFn>);
static_assert(RvalueAndThenCompat<StrResult, RvalueOnlyResultFn>);

// 编译期：map_result_t 以 `const T&` 推导返回类型，与 const 成员的实际调用形式一致
static_assert(std::is_same_v<res::detail::map_result_t<OverloadedMap, int>, long>);
static_assert(res::detail::MapCallable<OverloadedMap, int>);
static_assert(std::is_same_v<
              decltype(std::declval<const StrResult &>().map(
                  std::declval<OverloadedMap>())),
              res::Result<long, std::string>>);
static_assert(std::is_same_v<
              decltype(std::declval<const StrResult &>().map_or(
                  std::declval<long>(), std::declval<OverloadedMap>())),
              long>);
static_assert(std::is_same_v<
              decltype(std::declval<const StrResult &>().map_or_else(
                  std::declval<ErrorToLong>(), std::declval<OverloadedMap>())),
              long>);

// 编译期：错误侧 const 组合子以 `const E&` 判定，rvalue-only 可调用对象应被拒绝
static_assert(ErrCompat<StrResult, ErrorToLong>);
static_assert(!ErrCompat<StrResult, RvalueErrOnly>);
static_assert(MapOrElseCompat<StrResult, ErrorToLong>);
static_assert(!MapOrElseCompat<StrResult, RvalueErrOnly>);

// 编译期：map_err 返回类型以 `const E&` 推导，与 const 成员的实际调用形式一致；
// 不可调用 F 时 map_err_result_t 退化为 void（Clang 返回类型替换不硬错）。
static_assert(std::is_same_v<
              res::detail::map_err_result_t<OverloadedMapErr, std::string>,
              unsigned>);
static_assert(std::is_same_v<
              res::detail::map_err_result_t<RvalueErrOnly, std::string>,
              void>);
static_assert(std::is_same_v<
              decltype(std::declval<const StrResult &>().map_err(
                  std::declval<ErrorToLong>())),
              res::Result<int, long>>);

// 编译期：右值调用按值返回，左值调用仍返回 const 引用
static_assert(std::is_same_v<
              decltype(std::declval<StrResult>().inspect(&observe_ok)),
              StrResult>);
static_assert(!std::is_reference_v<
              decltype(std::declval<StrResult>().inspect(&observe_ok))>);
static_assert(std::is_same_v<
              decltype(std::declval<StrResult>().inspect_err(&observe_err)),
              StrResult>);
static_assert(!std::is_reference_v<
              decltype(std::declval<StrResult>().inspect_err(&observe_err))>);
static_assert(std::is_same_v<
              decltype(std::declval<StrResult &>().inspect(&observe_ok)),
              const StrResult &>);
static_assert(std::is_same_v<
              decltype(std::declval<StrResult &>().inspect_err(&observe_err)),
              const StrResult &>);

TEST_CASE("map transforms the Ok value")
{
    auto r = res::Result<int, std::string>::Ok(10).map(
        [](int v)
        { return v * 2; });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 20);
}

TEST_CASE("map passes through Err untouched")
{
    auto r = res::Result<int, std::string>::Err(std::string("e"))
                 .map(
                     [](int v)
                     { return v * 2; });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "e");
}

TEST_CASE("map returning void yields Result<void, E>")
{
    bool called = false;
    auto r = res::Result<int, std::string>::Ok(3).map(
        [&](int)
        { called = true; });
    CHECK(called);
    CHECK(r.is_ok());
}

TEST_CASE("map_err transforms the error")
{
    auto r = res::Result<int, std::string>::Err(std::string("e"))
                 .map_err([](const std::string &s)
                          { return s.size(); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == std::size_t{1});
}

TEST_CASE("map_err passes through Ok untouched")
{
    auto r = res::Result<int, std::string>::Ok(5)
                 .map_err([](const std::string &s)
                          { return s.size(); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 5);
}

TEST_CASE("map_err accepts a callable taking the error as const E&")
{
    auto r = StrResult::Err(std::string("boom")).map_err(
        [](const std::string &e)
        { return e.size(); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == std::size_t{4});

    // 值类别重载：const 成员按 `const E&` 调用，须选中 const 重载（unsigned）
    auto ov = StrResult::Err(std::string("abcd")).map_err(OverloadedMapErr{});
    CHECK(ov.is_err());
    CHECK(ov.unwrap_err() == 4u);
}

TEST_CASE("and_then chains on Ok")
{
    auto r = res::Result<int, std::string>::Ok(3).and_then(
        [](int x)
        { return res::Result<int, std::string>::Ok(x + 100); });
    CHECK(r.unwrap() == 103);
}

TEST_CASE("and_then short-circuits on Err")
{
    auto r = res::Result<int, std::string>::Err(std::string("e")).and_then([](int x)
                                                                           { return res::Result<int, std::string>::Ok(x + 100); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "e");
}

TEST_CASE("and_then rvalue overload accepts an rvalue-only callable")
{
    auto r = StrResult::Ok(3);
    auto out = std::move(r).and_then(RvalueOnlyResultFn{});
    CHECK(out.is_ok());
    CHECK(out.unwrap() == 3L);
}

TEST_CASE("or_else recovers from Err")
{
    auto r = res::Result<int, std::string>::Err(std::string("e"))
                 .or_else([](const std::string &)
                          { return res::Result<int, std::string>::Ok(0); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 0);
}

TEST_CASE("or_else passes Ok through unchanged")
{
    auto r = res::Result<int, std::string>::Ok(7).or_else(
        [](const std::string &)
        { return res::Result<int, std::string>::Ok(0); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 7);
}

TEST_CASE("or_else can change the error type")
{
    auto r = res::Result<int, std::string>::Err(std::string("e"))
                 .or_else([](const std::string &e)
                          { return res::Result<int, std::size_t>::Err(e.size()); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == std::size_t{1});
}

TEST_CASE("map_or returns transform on Ok, default on Err")
{
    auto ok = res::Result<int, std::string>::Ok(10);
    CHECK(ok.map_or(-1, [](int v)
                    { return v * 2; }) == 20);

    auto err = res::Result<int, std::string>::Err(std::string("e"));
    CHECK(err.map_or(-1, [](int v)
                     { return v * 2; }) == -1);
}

TEST_CASE("map_or_else picks transform or error fallback")
{
    auto ok = res::Result<int, std::string>::Ok(10);
    CHECK(ok.map_or_else(
              [](const std::string &)
              { return -1; }, [](int v)
              { return v * 2; }) == 20);

    auto err = res::Result<int, std::string>::Err(std::string("abc"));
    CHECK(err.map_or_else(
              [](const std::string &e)
              { return static_cast<int>(e.size()); },
              [](int v)
              { return v * 2; }) == 3);
}

TEST_CASE("inspect observes Ok value and returns self")
{
    int seen = 0;
    auto r = res::Result<int, std::string>::Ok(7);
    const auto &same = r.inspect(
        [&](int v)
        { seen = v; });
    CHECK(seen == 7);
    CHECK(&same == &r);

    seen = 0;
    auto e = res::Result<int, std::string>::Err(std::string("e"));
    e.inspect([&](int v)
              { seen = v; });
    CHECK(seen == 0); // not invoked on Err
}

TEST_CASE("inspect_err observes Err value and returns self")
{
    std::string seen;
    auto e = res::Result<int, std::string>::Err(std::string("boom"));
    e.inspect_err(
        [&](const std::string &v)
        { seen = v; });
    CHECK(seen == "boom");

    seen.clear();
    auto ok = res::Result<int, std::string>::Ok(1);
    ok.inspect_err(
        [&](const std::string &v)
        { seen = v; });
    CHECK(seen.empty()); // not invoked on Ok
}

TEST_CASE("inspect_err accepts callable taking E & (non-const lvalue ref)")
{
    auto e = res::Result<int, std::string>::Err(std::string("boom"));
    std::size_t n = 0;
    e.inspect_err(
        [&](const std::string &v)
        { n = v.size(); });
    CHECK(n == 4);
}

TEST_CASE("map throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    (void)std::move(r).unwrap();
    CHECK_THROWS_AS(
        (void)r.map(
            [](int v)
            { return v * 2; }),
        bad_access);
}

TEST_CASE("map_err throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    (void)std::move(r).unwrap();
    CHECK_THROWS_AS(
        (void)r.map_err(
            [](const std::string &e)
            { return e.size(); }),
        bad_access);
}

TEST_CASE("map_or_else throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    (void)std::move(r).unwrap();
    CHECK_THROWS_AS(
        (void)r.map_or_else(
            [](const std::string &e)
            { return static_cast<int>(e.size()); },
            [](int v)
            { return v * 2; }),
        bad_access);
}

TEST_CASE("map_or throws on moved Result")
{
    auto r = StrResult::Ok(5);
    (void)std::move(r).unwrap();
    CHECK_THROWS_AS(
        (void)r.map_or(-1, [](int v)
                       { return v * 2; }),
        bad_access);
}

TEST_CASE("inspect and inspect_err throw on moved Result")
{
    auto r = StrResult::Ok(5);
    (void)std::move(r).unwrap();

    CHECK_THROWS_AS((void)r.inspect(&observe_ok), bad_access);
    CHECK_THROWS_AS((void)std::move(r).inspect(&observe_ok), bad_access);
    CHECK_THROWS_AS((void)r.inspect_err(&observe_err), bad_access);
    CHECK_THROWS_AS((void)std::move(r).inspect_err(&observe_err), bad_access);
}

TEST_CASE("and_then throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    (void)std::move(r).unwrap();
    CHECK_THROWS_AS(
        (void)r.and_then(
            [](int x)
            { return res::Result<int, std::string>::Ok(x + 1); }),
        bad_access);
}

TEST_CASE("or_else throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    (void)std::move(r).unwrap();
    CHECK_THROWS_AS(
        (void)r.or_else(
            [](const std::string &)
            { return res::Result<int, std::string>::Ok(0); }),
        bad_access);
}

TEST_CASE("unwrap_or_else throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    (void)std::move(r).unwrap();
    CHECK_THROWS_AS(
        (void)r.unwrap_or_else(
            [](const std::string &)
            { return -1; }),
        bad_access);
}

TEST_CASE("operator== throws on moved Result")
{
    auto a = res::Result<int, std::string>::Ok(1);
    auto b = res::Result<int, std::string>::Ok(2);
    (void)std::move(a).unwrap();
    (void)std::move(b).unwrap();
    CHECK_THROWS_AS((void)(a == b), bad_access);
}

TEST_CASE("flatten collapses nested Result")
{
    auto inner = res::Result<int, std::string>::Ok(42);
    auto outer = res::Result<
        res::Result<int, std::string>,
        std::string>::Ok(std::move(inner));
    auto flat = outer.flatten();
    CHECK(flat.is_ok());
    CHECK(flat.unwrap() == 42);
}

TEST_CASE("flatten on Err propagates")
{
    auto outer = res::Result<
        res::Result<int, std::string>,
        std::string>::Err(std::string("e"));
    auto flat = outer.flatten();
    CHECK(flat.is_err());
    CHECK(flat.unwrap_err() == "e");
}

TEST_CASE("flatten rvalue moves inner Result")
{
    auto inner = res::Result<
        std::unique_ptr<int>,
        std::string>::Ok(std::unique_ptr<int>(new int(7)));
    auto outer = res::Result<
        res::Result<
            std::unique_ptr<int>,
            std::string>,
        std::string>::Ok(std::move(inner));
    auto flat = std::move(outer).flatten();
    CHECK(flat.is_ok());
    CHECK(*flat.unwrap() == 7);
}

TEST_CASE("flatten rvalue on Err propagates")
{
    auto outer = res::Result<
        res::Result<
            int,
            std::string>,
        std::string>::Err(std::string("e"));
    auto flat = std::move(outer).flatten();
    CHECK(flat.is_err());
    CHECK(flat.unwrap_err() == "e");
}

// 编译期：flatten() const& 的返回类型对 void/非 void 内层值类型都等于内层 Result
static_assert(std::is_same_v<
              decltype(std::declval<const res::Result<
                           res::Result<void, int>, int> &>()
                           .flatten()),
              res::Result<void, int>>);
static_assert(std::is_same_v<
              decltype(std::declval<const res::Result<
                           res::Result<int, std::string>, std::string> &>()
                           .flatten()),
              res::Result<int, std::string>>);

TEST_CASE("flatten preserves an inner Err when the inner value type is void")
{
    using InnerVoid = res::Result<void, int>;
    using OuterVoid = res::Result<InnerVoid, int>;

    auto err = OuterVoid::Ok(InnerVoid::Err(5));
    auto flat = err.flatten();
    CHECK(flat.is_err());
    CHECK(flat.unwrap_err() == 5);

    auto inner_ok = OuterVoid::Ok(InnerVoid::Ok());
    auto flat_ok = inner_ok.flatten();
    CHECK(flat_ok.is_ok());

    auto err_rv = OuterVoid::Ok(InnerVoid::Err(7));
    auto flat_rv = std::move(err_rv).flatten();
    CHECK(flat_rv.is_err());
    CHECK(flat_rv.unwrap_err() == 7);

    auto ok_rv = OuterVoid::Ok(InnerVoid::Ok());
    auto flat_ok_rv = std::move(ok_rv).flatten();
    CHECK(flat_ok_rv.is_ok());
}

TEST_CASE("flatten throws on moved Result")
{
    using Nested = res::Result<res::Result<int, std::string>, std::string>;
    auto r = Nested::Ok(res::Result<int, std::string>::Ok(1));
    (void)std::move(r).unwrap();
    CHECK_THROWS_AS((void)r.flatten(), bad_access);
    CHECK_THROWS_AS((void)std::move(r).flatten(), bad_access);
}

TEST_CASE("transpose throws on moved Result")
{
    using Nested = res::Result<res::Option<int>, std::string>;
    auto r = Nested::Ok(res::Option<int>::Some(1));
    (void)std::move(r).unwrap();
    CHECK_THROWS_AS((void)r.transpose(), bad_access);
    CHECK_THROWS_AS((void)std::move(r).transpose(), bad_access);
}

TEST_CASE("inspect rvalue observes only the active branch and returns by value")
{
    int ok_calls = 0;
    int err_calls = 0;

    auto ok = StrResult::Ok(7);
    auto moved_ok = std::move(ok).inspect(
        [&](int v)
        {
            ++ok_calls;
            CHECK(v == 7);
        });
    CHECK(ok_calls == 1);
    CHECK(moved_ok.is_ok());
    CHECK(moved_ok.unwrap() == 7);
    CHECK(ok.is_ok()); // source not marked moved

    auto er = StrResult::Err(std::string("e"));
    auto moved_err = std::move(er).inspect(
        [&](int v)
        {
            ++err_calls;
            (void)v;
        });
    CHECK(err_calls == 0); // not invoked on Err
    CHECK(ok_calls == 1);
    CHECK(moved_err.is_err());
    CHECK(er.is_err()); // source not marked moved
}

TEST_CASE("inspect_err rvalue observes only the error branch and returns by value")
{
    int calls = 0;

    auto er = StrResult::Err(std::string("boom"));
    auto moved_err = std::move(er).inspect_err(
        [&](const std::string &e)
        {
            ++calls;
            CHECK(e == "boom");
        });
    CHECK(calls == 1);
    CHECK(moved_err.is_err());
    CHECK(moved_err.unwrap_err() == "boom");
    CHECK(er.is_err()); // source not marked moved

    auto ok = StrResult::Ok(3);
    auto moved_ok = std::move(ok).inspect_err(
        [&](const std::string &e)
        {
            ++calls;
            (void)e;
        });
    CHECK(calls == 1); // not invoked on Ok
    CHECK(moved_ok.is_ok());
    CHECK(ok.is_ok());
}

TEST_CASE("inspect rvalue chains from a temporary Result")
{
    int seen = 0;
    auto r = make_ok(4)
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
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 40);
}

TEST_CASE("lvalue inspect keeps returning a reference to self")
{
    auto r = StrResult::Ok(1);
    const auto &same = r.inspect(&observe_ok);
    CHECK(&same == &r);
    CHECK(r.is_ok());
}

// 编译期：and_with 取 other 的成功类型 U 并保留 E；or_with 保留 T 并取 other 的错误类型 F
static_assert(std::is_same_v<
              decltype(std::declval<StrResult &>().and_with(
                  std::declval<res::Result<long, std::string>>())),
              res::Result<long, std::string>>);
static_assert(std::is_same_v<
              decltype(std::declval<StrResult &>().or_with(
                  std::declval<res::Result<int, std::size_t>>())),
              res::Result<int, std::size_t>>);
static_assert(std::is_same_v<
              decltype(std::declval<res::Result<void, std::string>>().and_with(
                  std::declval<res::Result<int, std::string>>())),
              res::Result<int, std::string>>);
static_assert(std::is_same_v<
              decltype(std::declval<res::Result<void, std::string>>().or_with(
                  std::declval<res::Result<void, std::size_t>>())),
              res::Result<void, std::size_t>>);

TEST_CASE("and_with returns other on Ok")
{
    auto r = res::Result<int, long>::Ok(3).and_with(
        res::Result<std::string, long>::Ok(std::string("x")));
    CHECK(r.is_ok());
    CHECK(r.unwrap() == "x");
}

TEST_CASE("and_with propagates the error on Err")
{
    auto r = StrResult::Err(std::string("e")).and_with(
        res::Result<long, std::string>::Ok(1L));
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "e");
}

TEST_CASE("and_with const lvalue copies other")
{
    res::Result<int, long> base = res::Result<int, long>::Ok(1);
    res::Result<std::string, long> other =
        res::Result<std::string, long>::Ok(std::string("x"));
    auto r = base.and_with(other);
    CHECK(r.unwrap() == "x");
    CHECK(other.is_ok()); // other left intact
}

TEST_CASE("and_with rvalue moves a move-only other")
{
    auto r = StrResult::Ok(1).and_with(
        res::Result<std::unique_ptr<int>, std::string>::Ok(
            std::unique_ptr<int>(new int(9))));
    CHECK(r.is_ok());
    CHECK(*r.unwrap() == 9);
}

TEST_CASE("or_with keeps Ok and replaces Err")
{
    auto kept = StrResult::Ok(3).or_with(res::Result<int, std::size_t>::Ok(9));
    CHECK(kept.is_ok());
    CHECK(kept.unwrap() == 3);

    auto recovered =
        StrResult::Err(std::string("e")).or_with(res::Result<int, std::size_t>::Ok(9));
    CHECK(recovered.is_ok());
    CHECK(recovered.unwrap() == 9);

    auto still_err = StrResult::Err(std::string("e"))
                         .or_with(res::Result<int, std::size_t>::Err(std::size_t{7}));
    CHECK(still_err.is_err());
    CHECK(still_err.unwrap_err() == std::size_t{7});
}

TEST_CASE("or_with const lvalue copies the Ok value")
{
    StrResult base = StrResult::Ok(3);
    auto r = base.or_with(res::Result<int, std::size_t>::Ok(9));
    CHECK(r.unwrap() == 3);
    CHECK(base.is_ok());
}

TEST_CASE("or_with rvalue moves a move-only Ok value")
{
    auto base = res::Result<std::unique_ptr<int>, std::string>::Ok(
        std::unique_ptr<int>(new int(3)));
    auto r = std::move(base).or_with(
        res::Result<std::unique_ptr<int>, std::size_t>::Err(std::size_t{1}));
    CHECK(r.is_ok());
    CHECK(*r.unwrap() == 3);
}

TEST_CASE("Result<void, E>::and_with yields other or propagates")
{
    auto r = res::Result<void, std::string>::Ok().and_with(
        res::Result<int, std::string>::Ok(7));
    CHECK(r.unwrap() == 7);

    auto e = res::Result<void, std::string>::Err(std::string("e")).and_with(
        res::Result<int, std::string>::Ok(7));
    CHECK(e.is_err());
    CHECK(e.unwrap_err() == "e");
}

TEST_CASE("Result<void, E>::or_with keeps Ok or takes other")
{
    auto ok = res::Result<void, std::string>::Ok().or_with(
        res::Result<void, std::size_t>::Err(std::size_t{1}));
    CHECK(ok.is_ok());

    auto rec = res::Result<void, std::string>::Err(std::string("e")).or_with(
        res::Result<void, std::size_t>::Err(std::size_t{1}));
    CHECK(rec.is_err());
    CHECK(rec.unwrap_err() == std::size_t{1});
}

TEST_CASE("and_with and or_with throw on moved Result")
{
    auto r = StrResult::Ok(5);
    (void)std::move(r).unwrap();
    CHECK_THROWS_AS(
        (void)r.and_with(res::Result<long, std::string>::Ok(1L)), bad_access);
    CHECK_THROWS_AS((void)r.or_with(res::Result<int, std::size_t>::Ok(9)), bad_access);
}

// 编译期：unwrap_err_or_else 返回 E，void 版接受无参可调用对象
static_assert(std::is_same_v<
              decltype(std::declval<const StrResult &>().unwrap_err_or_else(
                  &error_from_int)),
              std::string>);
static_assert(std::is_same_v<
              decltype(std::declval<const res::Result<void, std::string> &>()
                           .unwrap_err_or_else(&error_from_void)),
              std::string>);

TEST_CASE("unwrap_err_or_else returns the error on Err without invoking f")
{
    auto r = StrResult::Err(std::string("e"));
    int calls = 0;
    auto e = r.unwrap_err_or_else(
        [&](const int &)
        {
            ++calls;
            return std::string("fallback");
        });
    CHECK(e == "e");
    CHECK(calls == 0);
}

TEST_CASE("unwrap_err_or_else computes the error from the success value on Ok")
{
    auto r = StrResult::Ok(3);
    CHECK(r.unwrap_err_or_else(
              [](const int &v)
              { return std::string(v, 'x'); }) == "xxx");
    // callable taking T by value is also accepted
    CHECK(r.unwrap_err_or_else(
              [](int v)
              { return std::string(v, 'y'); }) == "yyy");
    CHECK(r.is_ok()); // receiver untouched
}

TEST_CASE("unwrap_err_or_else on Result<void, E>")
{
    auto ok = res::Result<void, std::string>::Ok();
    CHECK(ok.unwrap_err_or_else([] { return std::string("fallback"); }) == "fallback");

    auto err = res::Result<void, std::string>::Err(std::string("boom"));
    bool called = false;
    CHECK(err.unwrap_err_or_else(
              [&]
              {
                  called = true;
                  return std::string("nope");
              }) == "boom");
    CHECK(!called); // fallback not invoked on Err
}

TEST_CASE("unwrap_err_or_else works with a move-only success value")
{
    auto ok = res::Result<std::unique_ptr<int>, std::string>::Ok(
        std::unique_ptr<int>(new int(4)));
    CHECK(ok.unwrap_err_or_else(
              [](const std::unique_ptr<int> &p)
              { return std::string(*p, 'z'); }) == "zzzz");

    auto err = res::Result<std::unique_ptr<int>, std::string>::Err(std::string("bad"));
    CHECK(err.unwrap_err_or_else(
              [](const std::unique_ptr<int> &)
              { return std::string("x"); }) == "bad");
}

TEST_CASE("unwrap_err_or_else throws on moved Result")
{
    auto r = StrResult::Ok(5);
    (void)std::move(r).unwrap();
    CHECK_THROWS_AS(
        (void)r.unwrap_err_or_else(
            [](const int &)
            { return std::string("f"); }),
        bad_access);
}
