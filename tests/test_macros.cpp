#include <doctest/doctest.h>

#include <string>

#include "pjh_result/macros.hpp"
#include "pjh_result/result.hpp"

namespace res = pjh::result;

static res::Result<int, std::string> parse(bool good)
{
    if (!good)
        return res::Result<int, std::string>::Err(std::string("bad"));
    return res::Result<int, std::string>::Ok(10);
}

static res::Result<int, std::string> use_assign(bool good)
{
    ASSIGN_OR_RETURN(v, parse(good));
    return res::Result<int, std::string>::Ok(v + 1);
}

static res::Result<void, std::string> use_try(bool good)
{
    TRY(parse(good));
    return res::Result<void, std::string>::Ok();
}

TEST_CASE("ASSIGN_OR_RETURN unwraps on Ok, propagates on Err")
{
    CHECK(use_assign(true).unwrap() == 11);

    auto e = use_assign(false);
    CHECK(e.is_err());
    CHECK(e.unwrap_err() == "bad");
}

TEST_CASE("TRY continues on Ok, propagates on Err")
{
    CHECK(use_try(true).is_ok());

    auto e = use_try(false);
    CHECK(e.is_err());
    CHECK(e.unwrap_err() == "bad");
}
