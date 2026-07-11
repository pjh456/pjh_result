#include <doctest/doctest.h>

#include "pjh_result/option.hpp"

namespace res = pjh::result;
using bad_access = pjh::result::bad_result_access;

TEST_CASE("void Some is present and unwrap does not throw")
{
    auto o = res::Option<void>::Some();
    CHECK(o.is_some());
    CHECK_NOTHROW(o.unwrap());
}

TEST_CASE("void None is empty and unwrap throws")
{
    auto o = res::Option<void>::None();
    CHECK(o.is_none());
    CHECK_THROWS_AS(o.unwrap(), bad_access);
}

TEST_CASE("void option copies and assigns by state")
{
    auto a = res::Option<void>::Some();
    auto b = a;
    CHECK(b.is_some());
    b = res::Option<void>::None();
    CHECK(b.is_none());
}
