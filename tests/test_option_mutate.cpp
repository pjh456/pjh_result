#include <doctest/doctest.h>


#include "pjh_result/option.hpp"
#include "support/instrumented.hpp"

namespace res = pjh::result;
using pjh_test::InstanceCounter;

TEST_CASE("take moves the value out and leaves None")
{
    auto o = res::Option<int>::Some(5);
    auto taken = o.take();
    CHECK(taken.unwrap() == 5);
    CHECK(o.is_none());

    auto empty = res::Option<int>::None();
    CHECK(empty.take().is_none());
}

TEST_CASE("replace swaps the value and returns the old one")
{
    auto o = res::Option<int>::Some(1);
    auto old = o.replace(2);
    CHECK(old.unwrap() == 1);
    CHECK(o.unwrap() == 2);

    auto n = res::Option<int>::None();
    auto prev = n.replace(9);
    CHECK(prev.is_none());
    CHECK(n.unwrap() == 9);
}

TEST_CASE("insert overwrites and returns a reference")
{
    auto o = res::Option<int>::Some(1);
    int &r = o.insert(42);
    CHECK(r == 42);
    CHECK(o.unwrap() == 42);
    r = 43;
    CHECK(o.unwrap() == 43);
}

TEST_CASE("get_or_insert only inserts when None")
{
    auto none = res::Option<int>::None();
    CHECK(none.get_or_insert(7) == 7);
    CHECK(none.unwrap() == 7);

    auto some = res::Option<int>::Some(1);
    CHECK(some.get_or_insert(99) == 1); // already Some, keep
    CHECK(some.unwrap() == 1);
}

TEST_CASE("get_or_insert_default default-constructs when None")
{
    auto none = res::Option<int>::None();
    int &r = none.get_or_insert_default();
    CHECK(r == 0);
    CHECK(none.unwrap() == 0);
    r = 42;
    CHECK(none.unwrap() == 42);

    auto some = res::Option<int>::Some(7);
    CHECK(some.get_or_insert_default() == 7);
}

TEST_CASE("get_or_insert_with calls f when None")
{
    auto none = res::Option<int>::None();
    CHECK(none.get_or_insert_with([] { return 99; }) == 99);
    CHECK(none.unwrap() == 99);

    auto some = res::Option<int>::Some(5);
    bool called = false;
    CHECK(some.get_or_insert_with([&] { called = true; return -1; }) == 5);
    CHECK(!called);
}

TEST_CASE("mutations do not leak")
{
    InstanceCounter::reset();
    {
        auto o = res::Option<InstanceCounter>::Some(InstanceCounter{1});
        auto old = o.replace(InstanceCounter{2});
        o.insert(InstanceCounter{3});
        auto taken = o.take();
        CHECK(taken.unwrap().id == 3);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("take works on void option")
{
    auto o = res::Option<void>::Some();
    auto taken = o.take();
    CHECK(taken.is_some());
    CHECK(o.is_none());
}
