#include <doctest/doctest.h>

#include <cstddef>
#include <string>

#include "pjh_result/result.hpp"

namespace rp = pjh::result::utils;

TEST_CASE("map transforms the Ok value")
{
    auto r = rp::Result<int, std::string>::Ok(10).map([](int v) { return v * 2; });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 20);
}

TEST_CASE("map passes through Err untouched")
{
    auto r = rp::Result<int, std::string>::Err(std::string("e")).map([](int v) { return v * 2; });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "e");
}

TEST_CASE("map returning void yields Result<void, E>")
{
    bool called = false;
    auto r = rp::Result<int, std::string>::Ok(3).map([&](int) { called = true; });
    CHECK(called);
    CHECK(r.is_ok());
}

TEST_CASE("map_err transforms the error")
{
    auto r = rp::Result<int, std::string>::Err(std::string("e"))
                 .map_err([](const std::string &s) { return s.size(); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == std::size_t{1});
}

TEST_CASE("map_err passes through Ok untouched")
{
    auto r = rp::Result<int, std::string>::Ok(5)
                 .map_err([](const std::string &s) { return s.size(); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 5);
}

TEST_CASE("and_then chains on Ok")
{
    auto r = rp::Result<int, std::string>::Ok(3).and_then(
        [](int x) { return rp::Result<int, std::string>::Ok(x + 100); });
    CHECK(r.unwrap() == 103);
}

TEST_CASE("and_then short-circuits on Err")
{
    auto r = rp::Result<int, std::string>::Err(std::string("e")).and_then(
        [](int x) { return rp::Result<int, std::string>::Ok(x + 100); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "e");
}

TEST_CASE("or_else recovers from Err")
{
    auto r = rp::Result<int, std::string>::Err(std::string("e")).or_else(
        [](const std::string &) { return rp::Result<int, std::string>::Ok(0); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 0);
}

TEST_CASE("or_else passes Ok through unchanged")
{
    auto r = rp::Result<int, std::string>::Ok(7).or_else(
        [](const std::string &) { return rp::Result<int, std::string>::Ok(0); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 7);
}

TEST_CASE("or_else can change the error type")
{
    auto r = rp::Result<int, std::string>::Err(std::string("e")).or_else(
        [](const std::string &e) { return rp::Result<int, std::size_t>::Err(e.size()); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == std::size_t{1});
}

TEST_CASE("map_or returns transform on Ok, default on Err")
{
    auto ok = rp::Result<int, std::string>::Ok(10);
    CHECK(ok.map_or(-1, [](int v) { return v * 2; }) == 20);

    auto err = rp::Result<int, std::string>::Err(std::string("e"));
    CHECK(err.map_or(-1, [](int v) { return v * 2; }) == -1);
}

TEST_CASE("map_or_else picks transform or error fallback")
{
    auto ok = rp::Result<int, std::string>::Ok(10);
    CHECK(ok.map_or_else([](const std::string &) { return -1; }, [](int v) { return v * 2; }) == 20);

    auto err = rp::Result<int, std::string>::Err(std::string("abc"));
    CHECK(err.map_or_else([](const std::string &e) { return static_cast<int>(e.size()); },
                          [](int v) { return v * 2; }) == 3);
}

TEST_CASE("inspect observes Ok value and returns self")
{
    int seen = 0;
    auto r = rp::Result<int, std::string>::Ok(7);
    const auto &same = r.inspect([&](int v) { seen = v; });
    CHECK(seen == 7);
    CHECK(&same == &r);

    seen = 0;
    auto e = rp::Result<int, std::string>::Err(std::string("e"));
    e.inspect([&](int v) { seen = v; });
    CHECK(seen == 0); // not invoked on Err
}

TEST_CASE("inspect_err observes Err value and returns self")
{
    std::string seen;
    auto e = rp::Result<int, std::string>::Err(std::string("boom"));
    e.inspect_err([&](const std::string &v) { seen = v; });
    CHECK(seen == "boom");

    seen.clear();
    auto ok = rp::Result<int, std::string>::Ok(1);
    ok.inspect_err([&](const std::string &v) { seen = v; });
    CHECK(seen.empty()); // not invoked on Ok
}

