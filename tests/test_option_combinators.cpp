#include <doctest/doctest.h>

#include <memory>
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

TEST_CASE("filter rvalue works with move-only type")
{
    auto o = res::Option<std::unique_ptr<int>>::Some(std::unique_ptr<int>(new int(42)));
    auto kept = std::move(o).filter(
        [](const std::unique_ptr<int> &p)
        { return *p > 0; });
    CHECK(kept.is_some());
    CHECK(*kept.unwrap() == 42);

    auto empty = res::Option<std::unique_ptr<int>>::None();
    auto none = std::move(empty).filter(
        [](const std::unique_ptr<int> &)
        { return true; });
    CHECK(none.is_none());
}

TEST_CASE("flatten collapses nested Option")
{
    auto inner = res::Option<int>::Some(42);
    auto outer = res::Option<res::Option<int>>::Some(std::move(inner));
    auto flat = outer.flatten();
    CHECK(flat.is_some());
    CHECK(flat.unwrap() == 42);
}

TEST_CASE("flatten on None returns None")
{
    auto outer = res::Option<res::Option<int>>::None();
    auto flat = outer.flatten();
    CHECK(flat.is_none());
}

TEST_CASE("flatten on Some(None) returns None")
{
    auto outer = res::Option<res::Option<int>>::Some(res::Option<int>::None());
    auto flat = outer.flatten();
    CHECK(flat.is_none());
}

TEST_CASE("flatten rvalue moves inner Option")
{
    auto outer = res::Option<res::Option<std::unique_ptr<int>>>::Some(
        res::Option<std::unique_ptr<int>>::Some(std::unique_ptr<int>(new int(7))));
    auto flat = std::move(outer).flatten();
    CHECK(flat.is_some());
    CHECK(*flat.unwrap() == 7);
}

TEST_CASE("flatten rvalue on None returns None")
{
    auto outer = res::Option<res::Option<int>>::None();
    auto flat = std::move(outer).flatten();
    CHECK(flat.is_none());
}

TEST_CASE("zip pairs two Somes")
{
    auto a = res::Option<int>::Some(1);
    auto b = res::Option<int>::Some(2);
    auto z = a.zip(b);
    CHECK(z.is_some());
    CHECK(z.unwrap() == std::make_pair(1, 2));
}

TEST_CASE("zip with None returns None")
{
    auto a = res::Option<int>::Some(1);
    auto b = res::Option<int>::None();
    CHECK(a.zip(b).is_none());
    CHECK(res::Option<int>::None().zip(a).is_none());
}

TEST_CASE("zip rvalue moves values")
{
    auto a = res::Option<std::unique_ptr<int>>::Some(std::unique_ptr<int>(new int(7)));
    auto b = res::Option<std::unique_ptr<int>>::Some(std::unique_ptr<int>(new int(8)));
    auto z = std::move(a).zip(std::move(b));
    CHECK(z.is_some());
    CHECK(*z.unwrap().first == 7);
    CHECK(*z.unwrap().second == 8);
}

TEST_CASE("zip_with combines with a function")
{
    auto a = res::Option<int>::Some(3);
    auto b = res::Option<int>::Some(4);
    auto z = a.zip_with(b, [](int x, int y)
                        { return x + y; });
    CHECK(z.is_some());
    CHECK(z.unwrap() == 7);
}

TEST_CASE("zip_with with None returns None")
{
    auto a = res::Option<int>::Some(3);
    auto b = res::Option<int>::None();
    CHECK(a.zip_with(b, [](int x, int y)
                     { return x + y; })
              .is_none());
}

TEST_CASE("zip_with rvalue moves values into combiner")
{
    auto a = res::Option<std::string>::Some(std::string("hello"));
    auto b = res::Option<std::string>::Some(std::string("world"));
    auto z = std::move(a).zip_with(
        std::move(b),
        [](std::string x, std::string y)
        { return x + " " + y; });
    CHECK(z.is_some());
    CHECK(z.unwrap() == "hello world");
}
