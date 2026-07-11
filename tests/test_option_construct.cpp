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

TEST_CASE("is_some_and checks the value")
{
    auto some = res::Option<int>::Some(10);
    CHECK(some.is_some_and([](int v)
                           { return v > 5; }));
    CHECK_FALSE(some.is_some_and([](int v)
                                 { return v > 50; }));

    auto none = res::Option<int>::None();
    CHECK_FALSE(none.is_some_and([](int)
                                 { return true; }));
}
