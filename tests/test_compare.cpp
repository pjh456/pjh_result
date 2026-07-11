#include <doctest/doctest.h>

#include <string>

#include "pjh_result/result.hpp"

namespace rp = pjh::result::utils;

using IntResult = rp::Result<int, std::string>;
using VoidResult = rp::Result<void, std::string>;

TEST_CASE("equal Ok values compare equal")
{
    CHECK(IntResult::Ok(1) == IntResult::Ok(1));
    CHECK(IntResult::Ok(1) != IntResult::Ok(2));
}

TEST_CASE("equal Err values compare equal")
{
    CHECK(IntResult::Err(std::string("e")) == IntResult::Err(std::string("e")));
    CHECK(IntResult::Err(std::string("e")) != IntResult::Err(std::string("x")));
}

TEST_CASE("Ok and Err never compare equal")
{
    CHECK(IntResult::Ok(1) != IntResult::Err(std::string("e")));
}

TEST_CASE("void results compare by state and error")
{
    CHECK(VoidResult::Ok() == VoidResult::Ok());
    CHECK(VoidResult::Ok() != VoidResult::Err(std::string("e")));
    CHECK(VoidResult::Err(std::string("e")) == VoidResult::Err(std::string("e")));
    CHECK(VoidResult::Err(std::string("e")) != VoidResult::Err(std::string("x")));
}
