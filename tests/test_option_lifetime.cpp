#include <doctest/doctest.h>

#include <utility>

#include "pjh_result/option.hpp"
#include "support/instrumented.hpp"

namespace res = pjh::result;
using pjh_test::InstanceCounter;
using pjh_test::ThrowOnCopy;

TEST_CASE("no leak on scope exit")
{
    InstanceCounter::reset();
    {
        auto o = res::Option<InstanceCounter>::Some(InstanceCounter{1});
        CHECK(o.is_some());
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through copy and move")
{
    InstanceCounter::reset();
    {
        auto a = res::Option<InstanceCounter>::Some(InstanceCounter{2});
        auto b = a;
        auto c = std::move(a);
        CHECK(b.unwrap().id == 2);
        CHECK(c.unwrap().id == 2);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through assignment across states")
{
    InstanceCounter::reset();
    {
        auto a = res::Option<InstanceCounter>::Some(InstanceCounter{3});
        auto b = res::Option<InstanceCounter>::None();
        b = a;                 // None <- Some
        CHECK(b.unwrap().id == 3);
        a = res::Option<InstanceCounter>::None();
        b = std::move(a);      // Some <- None
        CHECK(b.is_none());
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("copy assignment offers strong exception guarantee")
{
    auto a = res::Option<ThrowOnCopy>::Some(ThrowOnCopy{9});
    auto b = res::Option<ThrowOnCopy>::None();

    CHECK_THROWS(b = a); // copying a's ThrowOnCopy throws
    CHECK(b.is_none());  // this unchanged
}
