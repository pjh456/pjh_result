#include <doctest/doctest.h>

#include <string>

#include "pjh_result/interop.hpp"

namespace res = pjh::result;

using IntResult = res::Result<int, std::string>;
using VoidResult = res::Result<void, std::string>;

TEST_CASE("ok converts Ok to Some, Err to None")
{
    auto some = res::ok(IntResult::Ok(7));
    CHECK(some.is_some());
    CHECK(some.unwrap() == 7);

    auto none = res::ok(IntResult::Err(std::string("e")));
    CHECK(none.is_none());
}

TEST_CASE("err converts Err to Some, Ok to None")
{
    auto some = res::err(IntResult::Err(std::string("boom")));
    CHECK(some.is_some());
    CHECK(some.unwrap() == "boom");

    auto none = res::err(IntResult::Ok(1));
    CHECK(none.is_none());
}

TEST_CASE("ok on a void result yields Option<void>")
{
    auto some = res::ok(VoidResult::Ok());
    CHECK(some.is_some());

    auto none = res::ok(VoidResult::Err(std::string("e")));
    CHECK(none.is_none());
}

TEST_CASE("ok found via ADL without qualification")
{
    auto o = ok(IntResult::Ok(3));
    CHECK(o.unwrap() == 3);
}

TEST_CASE("round-trips with Option::ok_or")
{
    auto r = IntResult::Ok(5);
    auto back = res::ok(r).ok_or(std::string("missing"));
    CHECK(back.is_ok());
    CHECK(back.unwrap() == 5);
}
