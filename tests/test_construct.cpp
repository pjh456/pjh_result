#include <doctest/doctest.h>

#include <string>

#include "pjh_result/result.hpp"

namespace rp = pjh::result::utils;

TEST_CASE("Ok factory holds value")
{
    auto r = rp::Result<int, std::string>::Ok(42);
    CHECK(r.is_ok());
    CHECK_FALSE(r.is_err());
    CHECK(r.unwrap() == 42);
}

TEST_CASE("Err factory holds error")
{
    auto r = rp::Result<int, std::string>::Err(std::string("boom"));
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "boom");
}

TEST_CASE("copy construction is independent")
{
    auto a = rp::Result<int, std::string>::Ok(7);
    auto b = a;
    CHECK(b.unwrap() == 7);
    CHECK(a.unwrap() == 7);
}

TEST_CASE("move construction preserves value")
{
    auto a = rp::Result<std::string, int>::Ok(std::string("hi"));
    auto b = std::move(a);
    CHECK(b.unwrap() == "hi");
}

TEST_CASE("Failure implicitly converts to Err")
{
    rp::Result<int, std::string> r = rp::Failure{std::string("bad")};
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "bad");
}
