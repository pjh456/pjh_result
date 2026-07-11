#include <doctest/doctest.h>

#include <cstddef>
#include <string>

#include "pjh_result/result.hpp"

namespace res = pjh::result;
using bad_access = pjh::result::bad_result_access;

TEST_CASE("map transforms the Ok value")
{
    auto r = res::Result<int, std::string>::Ok(10).map(
        [](int v)
        { return v * 2; });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 20);
}

TEST_CASE("map passes through Err untouched")
{
    auto r = res::Result<int, std::string>::Err(std::string("e"))
                 .map(
                     [](int v)
                     { return v * 2; });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "e");
}

TEST_CASE("map returning void yields Result<void, E>")
{
    bool called = false;
    auto r = res::Result<int, std::string>::Ok(3).map(
        [&](int)
        { called = true; });
    CHECK(called);
    CHECK(r.is_ok());
}

TEST_CASE("map_err transforms the error")
{
    auto r = res::Result<int, std::string>::Err(std::string("e"))
                 .map_err([](const std::string &s)
                          { return s.size(); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == std::size_t{1});
}

TEST_CASE("map_err passes through Ok untouched")
{
    auto r = res::Result<int, std::string>::Ok(5)
                 .map_err([](const std::string &s)
                          { return s.size(); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 5);
}

TEST_CASE("and_then chains on Ok")
{
    auto r = res::Result<int, std::string>::Ok(3).and_then(
        [](int x)
        { return res::Result<int, std::string>::Ok(x + 100); });
    CHECK(r.unwrap() == 103);
}

TEST_CASE("and_then short-circuits on Err")
{
    auto r = res::Result<int, std::string>::Err(std::string("e")).and_then([](int x)
                                                                           { return res::Result<int, std::string>::Ok(x + 100); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "e");
}

TEST_CASE("or_else recovers from Err")
{
    auto r = res::Result<int, std::string>::Err(std::string("e"))
                 .or_else([](const std::string &)
                          { return res::Result<int, std::string>::Ok(0); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 0);
}

TEST_CASE("or_else passes Ok through unchanged")
{
    auto r = res::Result<int, std::string>::Ok(7).or_else(
        [](const std::string &)
        { return res::Result<int, std::string>::Ok(0); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 7);
}

TEST_CASE("or_else can change the error type")
{
    auto r = res::Result<int, std::string>::Err(std::string("e"))
                 .or_else([](const std::string &e)
                          { return res::Result<int, std::size_t>::Err(e.size()); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == std::size_t{1});
}

TEST_CASE("map_or returns transform on Ok, default on Err")
{
    auto ok = res::Result<int, std::string>::Ok(10);
    CHECK(ok.map_or(-1, [](int v)
                    { return v * 2; }) == 20);

    auto err = res::Result<int, std::string>::Err(std::string("e"));
    CHECK(err.map_or(-1, [](int v)
                     { return v * 2; }) == -1);
}

TEST_CASE("map_or_else picks transform or error fallback")
{
    auto ok = res::Result<int, std::string>::Ok(10);
    CHECK(ok.map_or_else(
              [](const std::string &)
              { return -1; }, [](int v)
              { return v * 2; }) == 20);

    auto err = res::Result<int, std::string>::Err(std::string("abc"));
    CHECK(err.map_or_else(
              [](const std::string &e)
              { return static_cast<int>(e.size()); },
              [](int v)
              { return v * 2; }) == 3);
}

TEST_CASE("inspect observes Ok value and returns self")
{
    int seen = 0;
    auto r = res::Result<int, std::string>::Ok(7);
    const auto &same = r.inspect(
        [&](int v)
        { seen = v; });
    CHECK(seen == 7);
    CHECK(&same == &r);

    seen = 0;
    auto e = res::Result<int, std::string>::Err(std::string("e"));
    e.inspect([&](int v)
              { seen = v; });
    CHECK(seen == 0); // not invoked on Err
}

TEST_CASE("inspect_err observes Err value and returns self")
{
    std::string seen;
    auto e = res::Result<int, std::string>::Err(std::string("boom"));
    e.inspect_err(
        [&](const std::string &v)
        { seen = v; });
    CHECK(seen == "boom");

    seen.clear();
    auto ok = res::Result<int, std::string>::Ok(1);
    ok.inspect_err([&](const std::string &v)
                   { seen = v; });
    CHECK(seen.empty()); // not invoked on Ok
}

TEST_CASE("map throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    std::move(r).unwrap();
    CHECK_THROWS_AS(
        r.map(
            [](int v)
            { return v * 2; }),
        bad_access);
}

TEST_CASE("map_err throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    std::move(r).unwrap();
    CHECK_THROWS_AS(
        r.map_err(
            [](const std::string &e)
            { return e.size(); }),
        bad_access);
}

TEST_CASE("map_or_else throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    std::move(r).unwrap();
    CHECK_THROWS_AS(
        r.map_or_else(
            [](const std::string &e)
            { return static_cast<int>(e.size()); },
            [](int v)
            { return v * 2; }),
        bad_access);
}

TEST_CASE("and_then throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    std::move(r).unwrap();
    CHECK_THROWS_AS(
        r.and_then(
            [](int x)
            { return res::Result<int, std::string>::Ok(x + 1); }),
        bad_access);
}

TEST_CASE("or_else throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    std::move(r).unwrap();
    CHECK_THROWS_AS(
        r.or_else(
            [](const std::string &)
            { return res::Result<int, std::string>::Ok(0); }),
        bad_access);
}

TEST_CASE("unwrap_or_else throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    std::move(r).unwrap();
    CHECK_THROWS_AS(
        r.unwrap_or_else(
            [](const std::string &)
            { return -1; }),
        bad_access);
}

TEST_CASE("operator== throws on moved Result")
{
    auto a = res::Result<int, std::string>::Ok(1);
    auto b = res::Result<int, std::string>::Ok(2);
    std::move(a).unwrap();
    std::move(b).unwrap();
    CHECK_THROWS_AS((void)(a == b), bad_access);
}
