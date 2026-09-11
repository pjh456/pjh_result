#include <doctest/doctest.h>

#include <string>

#include "pjh_result/result.hpp"

namespace res = pjh::result;

TEST_CASE("Ok factory holds value")
{
    auto r = res::Result<int, std::string>::Ok(42);
    CHECK(r.is_ok());
    CHECK_FALSE(r.is_err());
    CHECK(r.unwrap() == 42);
}

TEST_CASE("Err factory holds error")
{
    auto r = res::Result<int, std::string>::Err(std::string("boom"));
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "boom");
}

TEST_CASE("copy construction is independent")
{
    auto a = res::Result<int, std::string>::Ok(7);
    auto b = a;
    CHECK(b.unwrap() == 7);
    CHECK(a.unwrap() == 7);
}

TEST_CASE("move construction preserves value")
{
    auto a = res::Result<std::string, int>::Ok(std::string("hi"));
    auto b = std::move(a);
    CHECK(b.unwrap() == "hi");
}

TEST_CASE("Failure implicitly converts to Err")
{
    res::Result<int, std::string> r = res::Failure{std::string("bad")};
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "bad");
}

namespace
{
    // Task 36: reference `T` / `E` and `E = void` must be rejected at the class
    // constraint level instead of hard-erroring deep inside the tagged union.
    template <typename T, typename E>
    concept ResultInstantiable = requires { typename res::Result<T, E>; };
}

static_assert(ResultInstantiable<int, std::string>);
static_assert(ResultInstantiable<void, std::string>);
static_assert(!ResultInstantiable<int &, std::string>);
static_assert(!ResultInstantiable<const int &, std::string>);
static_assert(!ResultInstantiable<int, std::string &>);
static_assert(!ResultInstantiable<int, const std::string &>);
static_assert(!ResultInstantiable<int, void>);
static_assert(!ResultInstantiable<void, void>);
