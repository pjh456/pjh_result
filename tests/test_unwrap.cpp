#include <doctest/doctest.h>

#include <string>
#include <utility>

#include "pjh_result/result.hpp"

namespace res = pjh::result;
using bad_access = pjh::result::bad_result_access;

TEST_CASE("unwrap returns value on Ok, throws on Err")
{
    auto ok = res::Result<int, std::string>::Ok(1);
    CHECK(ok.unwrap() == 1);

    auto err = res::Result<int, std::string>::Err(std::string("e"));
    CHECK_THROWS_AS((void)err.unwrap(), bad_access);
}

TEST_CASE("unwrap_err returns error on Err, throws on Ok")
{
    auto err = res::Result<int, std::string>::Err(std::string("e"));
    CHECK(err.unwrap_err() == "e");

    auto ok = res::Result<int, std::string>::Ok(1);
    CHECK_THROWS_AS((void)ok.unwrap_err(), bad_access);
}

TEST_CASE("unwrap_or returns fallback on Err")
{
    auto err = res::Result<std::string, int>::Err(5);
    CHECK(err.unwrap_or(std::string("fallback")) == "fallback");

    auto ok = res::Result<std::string, int>::Ok(std::string("hi"));
    CHECK(ok.unwrap_or(std::string("x")) == "hi");
}

TEST_CASE("unwrap_err_or returns fallback on Ok")
{
    auto ok = res::Result<int, std::string>::Ok(1);
    CHECK(ok.unwrap_err_or(std::string("def")) == "def");

    auto err = res::Result<int, std::string>::Err(std::string("real"));
    CHECK(err.unwrap_err_or(std::string("def")) == "real");
}

TEST_CASE("rvalue unwrap moves the value out")
{
    auto ok = res::Result<std::string, int>::Ok(std::string("movable"));
    std::string s = std::move(ok).unwrap();
    CHECK(s == "movable");
}

TEST_CASE("rvalue unwrap leaves Result in moved state")
{
    auto r = res::Result<int, std::string>::Ok(42);
    int v = std::move(r).unwrap();
    CHECK(v == 42);
    CHECK_FALSE(r.is_ok());
    CHECK_FALSE(r.is_err());
    CHECK_THROWS_AS((void)r.unwrap(), bad_access);
}

TEST_CASE("rvalue unwrap_err leaves Result in moved state")
{
    auto r = res::Result<int, std::string>::Err(std::string("e"));
    std::string e = std::move(r).unwrap_err();
    CHECK(e == "e");
    CHECK_FALSE(r.is_ok());
    CHECK_FALSE(r.is_err());
    CHECK_THROWS_AS((void)r.unwrap_err(), bad_access);
}

TEST_CASE("rvalue expect leaves Result in moved state")
{
    auto r = res::Result<int, std::string>::Ok(7);
    int v = std::move(r).expect("ok");
    CHECK(v == 7);
    CHECK_FALSE(r.is_ok());
    CHECK_THROWS_AS((void)r.expect("used after moved"), bad_access);
}

TEST_CASE("rvalue expect_err leaves Result in moved state")
{
    auto r = res::Result<int, std::string>::Err(std::string("err"));
    std::string e = std::move(r).expect_err("err");
    CHECK(e == "err");
    CHECK_FALSE(r.is_err());
    CHECK_THROWS_AS((void)r.expect_err("used after moved"), bad_access);
}

TEST_CASE("expect returns value on Ok, throws custom message on Err")
{
    auto ok = res::Result<int, std::string>::Ok(5);
    CHECK(ok.expect("should be ok") == 5);

    auto err = res::Result<int, std::string>::Err(std::string("e"));
    CHECK_THROWS_AS((void)err.expect("boom"), bad_access);
}

TEST_CASE("expect_err returns error on Err, throws custom message on Ok")
{
    auto err = res::Result<int, std::string>::Err(std::string("e"));
    CHECK(err.expect_err("should be err") == "e");

    auto ok = res::Result<int, std::string>::Ok(5);
    CHECK_THROWS_AS((void)ok.expect_err("boom"), bad_access);
}

TEST_CASE("unwrap_or_else computes fallback from the error")
{
    auto err = res::Result<int, std::string>::Err(std::string("xyz"));
    CHECK(err.unwrap_or_else(
              [](const std::string &e)
              { return static_cast<int>(e.size()); }) == 3);

    auto ok = res::Result<int, std::string>::Ok(7);
    CHECK(ok.unwrap_or_else(
              [](const std::string &)
              { return 0; }) == 7);
}

TEST_CASE("unwrap_or_else accepts callable taking E by value or const ref")
{
    auto err = res::Result<int, std::string>::Err(std::string("abcd"));

    auto r1 = err.unwrap_or_else(
        [](const std::string &e)
        { return static_cast<int>(e.size()); });
    CHECK(r1 == 4);

    auto r2 = err.unwrap_or_else(
        [](std::string e)
        { return static_cast<int>(e.size()); });
    CHECK(r2 == 4);
}

TEST_CASE("unwrap_or_default returns default-constructed T on Err")
{
    auto err = res::Result<int, std::string>::Err(std::string("e"));
    CHECK(err.unwrap_or_default() == 0);

    auto ok = res::Result<int, std::string>::Ok(9);
    CHECK(ok.unwrap_or_default() == 9);
}
