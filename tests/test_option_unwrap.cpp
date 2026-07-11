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
