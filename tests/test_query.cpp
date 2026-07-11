#include <doctest/doctest.h>

#include <string>

#include "pjh_result/result.hpp"

namespace res = pjh::result;

TEST_CASE("is_ok_and checks the success value")
{
    auto ok = res::Result<int, std::string>::Ok(10);
    CHECK(ok.is_ok_and(
        [](int v)
        { return v > 5; }));
    CHECK_FALSE(ok.is_ok_and(
        [](int v)
        { return v > 50; }));

    auto err = res::Result<int, std::string>::Err(std::string("e"));
    CHECK_FALSE(err.is_ok_and(
        [](int)
        { return true; }));
}

TEST_CASE("is_err_and checks the error value")
{
    auto err = res::Result<int, std::string>::Err(std::string("boom"));
    CHECK(err.is_err_and(
        [](const std::string &e)
        { return e.size() == 4; }));
    CHECK_FALSE(err.is_err_and(
        [](const std::string &e)
        { return e.empty(); }));

    auto ok = res::Result<int, std::string>::Ok(1);
    CHECK_FALSE(ok.is_err_and(
        [](const std::string &)
        { return true; }));
}

TEST_CASE("is_ok_and on void result uses a nullary predicate")
{
    auto ok = res::Result<void, std::string>::Ok();
    CHECK(ok.is_ok_and(
        []()
        { return true; }));
    CHECK_FALSE(ok.is_ok_and(
        []()
        { return false; }));
}

TEST_CASE("contains matches Ok value")
{
    auto ok = res::Result<int, std::string>::Ok(42);
    CHECK(ok.contains(42));
    CHECK_FALSE(ok.contains(0));

    auto err = res::Result<int, std::string>::Err(std::string("e"));
    CHECK_FALSE(err.contains(42));
}

TEST_CASE("contains_err matches Err value")
{
    auto err = res::Result<int, std::string>::Err(std::string("boom"));
    CHECK(err.contains_err(std::string("boom")));
    CHECK_FALSE(err.contains_err(std::string("other")));

    auto ok = res::Result<int, std::string>::Ok(1);
    CHECK_FALSE(ok.contains_err(std::string("x")));
}
