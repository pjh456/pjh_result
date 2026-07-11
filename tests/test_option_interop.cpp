#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "pjh_result/option.hpp"

namespace res = pjh::result;

TEST_CASE("ok_or converts Some to Ok, None to Err")
{
    auto s = res::Option<int>::Some(7);
    auto r = s.ok_or(std::string("missing"));
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 7);

    auto n = res::Option<int>::None();
    auto e = n.ok_or(std::string("missing"));
    CHECK(e.is_err());
    CHECK(e.unwrap_err() == "missing");
}

TEST_CASE("ok_or_else lazily computes the error")
{
    auto s = res::Option<int>::Some(7);
    auto r = s.ok_or_else(
        []()
        { return std::string("missing"); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 7);

    auto n = res::Option<int>::None();
    auto e = n.ok_or_else(
        []()
        { return std::string("computed"); });
    CHECK(e.is_err());
    CHECK(e.unwrap_err() == "computed");
}

TEST_CASE("ok_or works with void Option")
{
    auto s = res::Option<void>::Some();
    auto r = s.ok_or(std::string("missing"));
    CHECK(r.is_ok());
    r.unwrap(); // returns void, must not throw

    auto n = res::Option<void>::None();
    auto e = n.ok_or(std::string("missing"));
    CHECK(e.is_err());
    CHECK(e.unwrap_err() == "missing");
}

TEST_CASE("ok_or works with move-only error type")
{
    auto s = res::Option<int>::Some(7);
    auto r = s.ok_or(std::unique_ptr<int>(new int(42)));
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 7);

    auto n = res::Option<int>::None();
    auto e = n.ok_or(std::unique_ptr<int>(new int(99)));
    CHECK(e.is_err());
    CHECK(*e.unwrap_err() == 99);
}
