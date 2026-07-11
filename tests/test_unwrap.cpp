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
    CHECK_THROWS_AS((void)err.unwrap(), bad_access);
}

TEST_CASE("unwrap_err returns error on Err, throws on Ok")
{
    auto err = rp::Result<int, std::string>::Err(std::string("e"));
    CHECK(err.unwrap_err() == "e");

    auto ok = rp::Result<int, std::string>::Ok(1);
    CHECK_THROWS_AS((void)ok.unwrap_err(), bad_access);
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

TEST_CASE("expect returns value on Ok, throws custom message on Err")
{
    auto ok = rp::Result<int, std::string>::Ok(5);
    CHECK(ok.expect("should be ok") == 5);

    auto err = rp::Result<int, std::string>::Err(std::string("e"));
    CHECK_THROWS_AS((void)err.expect("boom"), bad_access);
}

TEST_CASE("expect_err returns error on Err, throws custom message on Ok")
{
    auto err = rp::Result<int, std::string>::Err(std::string("e"));
    CHECK(err.expect_err("should be err") == "e");

    auto ok = rp::Result<int, std::string>::Ok(5);
    CHECK_THROWS_AS((void)ok.expect_err("boom"), bad_access);
}

TEST_CASE("unwrap_or_else computes fallback from the error")
{
    auto err = rp::Result<int, std::string>::Err(std::string("xyz"));
    CHECK(err.unwrap_or_else([](const std::string &e) { return static_cast<int>(e.size()); }) == 3);

    auto ok = rp::Result<int, std::string>::Ok(7);
    CHECK(ok.unwrap_or_else([](const std::string &) { return 0; }) == 7);
}

TEST_CASE("unwrap_or_default returns default-constructed T on Err")
{
    auto err = rp::Result<int, std::string>::Err(std::string("e"));
    CHECK(err.unwrap_or_default() == 0);

    auto ok = rp::Result<int, std::string>::Ok(9);
    CHECK(ok.unwrap_or_default() == 9);
}
