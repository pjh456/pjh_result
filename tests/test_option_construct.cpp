#include <doctest/doctest.h>

#include <string>
#include <utility>

#include "pjh_result/option.hpp"

namespace res = pjh::result;

TEST_CASE("Some holds a value")
{
    auto o = res::Option<int>::Some(42);
    CHECK(o.is_some());
    CHECK_FALSE(o.is_none());
    CHECK(o.unwrap() == 42);
}

TEST_CASE("None holds nothing")
{
    auto o = res::Option<int>::None();
    CHECK(o.is_none());
    CHECK_FALSE(o.is_some());
}

TEST_CASE("copy construction is independent")
{
    auto a = res::Option<int>::Some(7);
    auto b = a;
    CHECK(b.unwrap() == 7);
    CHECK(a.unwrap() == 7);
}

TEST_CASE("move construction preserves value")
{
    auto a = res::Option<std::string>::Some(std::string("hi"));
    auto b = std::move(a);
    CHECK(b.unwrap() == "hi");
}

TEST_CASE("copy of None stays None")
{
    auto a = res::Option<int>::None();
    auto b = a;
    CHECK(b.is_none());
}
