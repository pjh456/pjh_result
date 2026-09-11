#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include "pjh_result/result.hpp"

namespace res = pjh::result;
using bad_access = pjh::result::bad_result_access;

namespace
{
    // Task 24: invocable only with one argument. The void success branch is invoked
    // with no arguments, so the public API must reject it cleanly instead of
    // hard-erroring while substituting the return type on Clang.
    struct NonNullary
    {
        int operator()(int) const;
    };

    using VoidResult = res::Result<void, std::string>;

    template <typename R, typename F>
    concept MapCompat = requires(const R &r, F f) {
        r.map(f);
    };

    template <typename R, typename F>
    concept ConstAndThenCompat = requires(const R &r, F f) {
        r.and_then(f);
    };

    template <typename R, typename F>
    concept RvalueAndThenCompat = requires(R &&r, F f) {
        std::move(r).and_then(f);
    };
}

// 编译期：void 分支的返回类型 trait 对不可调用（非 nullary）的 F 退化为 void，
// 而不是硬错；随后由 MapCallable / CrefResultFn / ValueResultFn 干净拒绝。
static_assert(std::is_same_v<res::detail::map_result_t<NonNullary, void>, void>);
static_assert(std::is_same_v<res::detail::cref_result_t<NonNullary, void>, void>);
static_assert(std::is_same_v<res::detail::value_result_t<NonNullary, void>, void>);
static_assert(!MapCompat<VoidResult, NonNullary>);
static_assert(!ConstAndThenCompat<VoidResult, NonNullary>);
static_assert(!RvalueAndThenCompat<VoidResult, NonNullary>);

TEST_CASE("void Ok is ok and unwrap does not throw")
{
    auto r = res::Result<void, std::string>::Ok();
    CHECK(r.is_ok());
    CHECK_NOTHROW(r.unwrap());
}

TEST_CASE("void Err holds error and unwrap throws")
{
    auto r = res::Result<void, std::string>::Err(std::string("e"));
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "e");
    CHECK_THROWS_AS(r.unwrap(), bad_access);
}

TEST_CASE("void map value->void chains")
{
    int seen = 0;
    auto r = res::Result<void, std::string>::Ok().map([&]() { seen = 1; });
    CHECK(seen == 1);
    CHECK(r.is_ok());
}

TEST_CASE("void map to value")
{
    auto r = res::Result<void, std::string>::Ok().map([]() { return 99; });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 99);
}

TEST_CASE("void map_err transforms the error")
{
    auto r = res::Result<void, std::string>::Err(std::string("e"))
                 .map_err([](const std::string &s) { return s.size(); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == std::size_t{1});
}

TEST_CASE("void and_then chains into a valued Result")
{
    auto r = res::Result<void, std::string>::Ok().and_then(
        []() { return res::Result<int, std::string>::Ok(7); });
    CHECK(r.unwrap() == 7);
}
