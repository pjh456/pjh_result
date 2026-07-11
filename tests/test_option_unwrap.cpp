#include <doctest/doctest.h>

#include <string>
#include <utility>

#include "pjh_result/option.hpp"

namespace res = pjh::result;
using bad_access = pjh::result::bad_result_access;

TEST_CASE("unwrap returns value on Some, throws on None")
{
    auto some = res::Option<int>::Some(5);
    CHECK(some.unwrap() == 5);

    auto none = res::Option<int>::None();
    CHECK_THROWS_AS((void)none.unwrap(), bad_access);
}

TEST_CASE("const unwrap reads the value")
{
    const auto some = res::Option<int>::Some(9);
    CHECK(some.unwrap() == 9);
}

TEST_CASE("rvalue unwrap moves the value out")
{
    auto some = res::Option<std::string>::Some(std::string("movable"));
    std::string s = std::move(some).unwrap();
    CHECK(s == "movable");
}

TEST_CASE("rvalue unwrap leaves Option None")
{
    auto o = res::Option<int>::Some(42);
    int v = std::move(o).unwrap();
    CHECK(v == 42);
    CHECK(o.is_none());
    CHECK_THROWS_AS((void)o.unwrap(), bad_access);
}

TEST_CASE("rvalue expect leaves Option None")
{
    auto o = res::Option<int>::Some(7);
    int v = std::move(o).expect("some");
    CHECK(v == 7);
    CHECK(o.is_none());
    CHECK_THROWS_AS((void)o.expect("used after moved"), bad_access);
}

TEST_CASE("expect returns value on Some, throws custom message on None")
{
    auto some = res::Option<int>::Some(5);
    CHECK(some.expect("should be some") == 5);

    auto none = res::Option<int>::None();
    CHECK_THROWS_AS((void)none.expect("boom"), bad_access);
}

TEST_CASE("unwrap_or returns fallback on None")
{
    CHECK(res::Option<int>::None().unwrap_or(-1) == -1);
    CHECK(res::Option<int>::Some(7).unwrap_or(-1) == 7);
}

TEST_CASE("unwrap_or_else computes fallback on None")
{
    CHECK(res::Option<int>::None().unwrap_or_else(
              []()
              { return 42; }) == 42);
    CHECK(res::Option<int>::Some(7).unwrap_or_else(
              []()
              { return 42; }) == 7);
}

TEST_CASE("unwrap_or_default returns default-constructed T on None")
{
    CHECK(res::Option<int>::None().unwrap_or_default() == 0);
    CHECK(res::Option<int>::Some(9).unwrap_or_default() == 9);
}
