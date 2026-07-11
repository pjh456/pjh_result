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
