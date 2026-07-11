#include <doctest/doctest.h>

#include <string>
#include <utility>

#include "pjh_result/result.hpp"

namespace rp = pjh::result::utils;
using bad_access = pjh::result::utils::result_helper::bad_result_access;

TEST_CASE("unwrap returns value on Ok, throws on Err")
{
    auto ok = rp::Result<int, std::string>::Ok(1);
    CHECK(ok.unwrap() == 1);

    auto err = rp::Result<int, std::string>::Err(std::string("e"));
    CHECK_THROWS_AS(err.unwrap(), bad_access);
}

TEST_CASE("unwrap_err returns error on Err, throws on Ok")
{
    auto err = rp::Result<int, std::string>::Err(std::string("e"));
    CHECK(err.unwrap_err() == "e");

    auto ok = rp::Result<int, std::string>::Ok(1);
    CHECK_THROWS_AS(ok.unwrap_err(), bad_access);
}

TEST_CASE("unwrap_or returns fallback on Err")
{
    auto err = rp::Result<std::string, int>::Err(5);
    CHECK(err.unwrap_or(std::string("fallback")) == "fallback");

    auto ok = rp::Result<std::string, int>::Ok(std::string("hi"));
    CHECK(ok.unwrap_or(std::string("x")) == "hi");
}

TEST_CASE("unwrap_err_or returns fallback on Ok")
{
    auto ok = rp::Result<int, std::string>::Ok(1);
    CHECK(ok.unwrap_err_or(std::string("def")) == "def");

    auto err = rp::Result<int, std::string>::Err(std::string("real"));
    CHECK(err.unwrap_err_or(std::string("def")) == "real");
}

TEST_CASE("rvalue unwrap moves the value out")
{
    auto ok = rp::Result<std::string, int>::Ok(std::string("movable"));
    std::string s = std::move(ok).unwrap();
    CHECK(s == "movable");
}
