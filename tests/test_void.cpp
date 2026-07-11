#include <doctest/doctest.h>

#include <cstddef>
#include <string>

#include "pjh_result/result.hpp"

namespace res = pjh::result;
using bad_access = pjh::result::bad_result_access;

TEST_CASE("void Ok is ok and unwrap does not throw")
{
    auto r = res::Result<void, std::string>::Ok();
    CHECK(r.is_ok());
    CHECK_NOTHROW(r.unwrap());
}

TEST_CASE("void Err holds error and unwrap throws")
{
    auto r = res::Result<void, std::string>::Err(std::string("e"));
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "e");
    CHECK_THROWS_AS(r.unwrap(), bad_access);
}

TEST_CASE("void map value->void chains")
{
    int seen = 0;
    auto r = res::Result<void, std::string>::Ok().map([&]() { seen = 1; });
    CHECK(seen == 1);
    CHECK(r.is_ok());
}

TEST_CASE("void map to value")
{
    auto r = res::Result<void, std::string>::Ok().map([]() { return 99; });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 99);
}

TEST_CASE("void map_err transforms the error")
{
    auto r = res::Result<void, std::string>::Err(std::string("e"))
                 .map_err([](const std::string &s) { return s.size(); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == std::size_t{1});
}

TEST_CASE("void and_then chains into a valued Result")
{
    auto r = res::Result<void, std::string>::Ok().and_then(
        []() { return res::Result<int, std::string>::Ok(7); });
    CHECK(r.unwrap() == 7);
}
