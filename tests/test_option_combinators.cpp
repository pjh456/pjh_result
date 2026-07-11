#include <doctest/doctest.h>

#include <string>

#include "pjh_result/option.hpp"

namespace res = pjh::result;

TEST_CASE("map transforms Some, passes None through")
{
    auto s = res::Option<int>::Some(10).map(
        [](int v)
        { return v * 2; });
    CHECK(s.is_some());
    CHECK(s.unwrap() == 20);

    auto n = res::Option<int>::None().map(
        [](int v)
        { return v * 2; });
    CHECK(n.is_none());
}

TEST_CASE("map can change the value type")
{
    auto s = res::Option<int>::Some(3).map(
        [](int v)
        { return std::string(v, 'x'); });
    CHECK(s.unwrap() == "xxx");
}

TEST_CASE("map_or and map_or_else collapse to a value")
{
    CHECK(res::Option<int>::Some(10).map_or(
              -1,
              [](int v)
              { return v * 2; }) == 20);
    CHECK(res::Option<int>::None().map_or(
              -1,
              [](int v)
              { return v * 2; }) == -1);

    CHECK(res::Option<int>::Some(10).map_or_else(
              []()
              { return -1; }, [](int v)
              { return v * 2; }) == 20);
    CHECK(res::Option<int>::None().map_or_else(
              []()
              { return -1; }, [](int v)
              { return v * 2; }) == -1);
}

TEST_CASE("inspect observes Some and returns self")
{
    int seen = 0;
    auto o = res::Option<int>::Some(7);
    const auto &same = o.inspect(
        [&](int v)
        { seen = v; });
    CHECK(seen == 7);
    CHECK(&same == &o);

    seen = 0;
    res::Option<int>::None().inspect(
        [&](int v)
        { seen = v; });
    CHECK(seen == 0);
}

TEST_CASE("and_then chains option-returning operations")
{
    auto s = res::Option<int>::Some(3).and_then(
        [](int v)
        { return res::Option<int>::Some(v + 100); });
    CHECK(s.unwrap() == 103);

    auto n = res::Option<int>::None().and_then(
        [](int v)
        { return res::Option<int>::Some(v + 100); });
    CHECK(n.is_none());
}

TEST_CASE("or_else recovers from None")
{
    auto r = res::Option<int>::None().or_else(
        []()
        { return res::Option<int>::Some(0); });
    CHECK(r.unwrap() == 0);

    auto keep = res::Option<int>::Some(7).or_else(
        []()
        { return res::Option<int>::Some(0); });
    CHECK(keep.unwrap() == 7);
}

TEST_CASE("filter keeps or drops the value")
{
    auto kept = res::Option<int>::Some(10).filter(
        [](int v)
        { return v > 5; });
    CHECK(kept.is_some());
    CHECK(kept.unwrap() == 10);

    auto dropped = res::Option<int>::Some(3).filter(
        [](int v)
        { return v > 5; });
    CHECK(dropped.is_none());

    auto none = res::Option<int>::None().filter(
        [](int)
        { return true; });
    CHECK(none.is_none());
}
