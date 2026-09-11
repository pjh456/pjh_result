#include <doctest/doctest.h>

#include <concepts>
#include <memory>
#include <string>
#include <utility>

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

TEST_CASE("member ok on const lvalue copies and leaves source unchanged")
{
    const IntResult r = IntResult::Ok(7);
    auto o = r.ok();
    CHECK(o.is_some());
    CHECK(o.unwrap() == 7);
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 7);

    const IntResult e = IntResult::Err(std::string("e"));
    CHECK(e.ok().is_none());
}

TEST_CASE("member err on const lvalue copies and leaves source unchanged")
{
    const IntResult r = IntResult::Err(std::string("boom"));
    auto o = r.err();
    CHECK(o.is_some());
    CHECK(o.unwrap() == "boom");
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "boom");

    const IntResult ok = IntResult::Ok(1);
    CHECK(ok.err().is_none());
}

TEST_CASE("member ok on rvalue moves value out and marks source moved")
{
    IntResult r = IntResult::Ok(7);
    auto o = std::move(r).ok();
    CHECK(o.unwrap() == 7);
    CHECK(r.is_moved());

    IntResult e = IntResult::Err(std::string("e"));
    auto none = std::move(e).ok();
    CHECK(none.is_none());
    CHECK(e.is_moved());
}

TEST_CASE("member err on rvalue moves error out and marks source moved")
{
    IntResult r = IntResult::Err(std::string("boom"));
    auto o = std::move(r).err();
    CHECK(o.unwrap() == "boom");
    CHECK(r.is_moved());

    IntResult ok = IntResult::Ok(1);
    auto none = std::move(ok).err();
    CHECK(none.is_none());
    CHECK(ok.is_moved());
}

TEST_CASE("member ok/err move a non-trivial value")
{
    using PtrResult = res::Result<std::unique_ptr<int>, std::string>;
    PtrResult r = PtrResult::Ok(std::make_unique<int>(9));
    auto o = std::move(r).ok();
    REQUIRE(o.is_some());
    CHECK(*o.unwrap() == 9);
    CHECK(r.is_moved());

    using PtrErr = res::Result<int, std::unique_ptr<int>>;
    PtrErr e = PtrErr::Err(std::make_unique<int>(3));
    auto oe = std::move(e).err();
    REQUIRE(oe.is_some());
    CHECK(*oe.unwrap() == 3);
    CHECK(e.is_moved());
}

TEST_CASE("member ok/err throw on a moved Result")
{
    using bad_access = res::bad_result_access;
    IntResult r = IntResult::Ok(7);
    CHECK(std::move(r).unwrap() == 7);
    CHECK(r.is_moved());
    CHECK_THROWS_AS((void)r.ok(), bad_access);
    CHECK_THROWS_AS((void)r.err(), bad_access);
}

TEST_CASE("member ok chains into Option combinators")
{
    auto doubled = IntResult::Ok(4).ok().map([](int v) { return v * 2; });
    CHECK(doubled.unwrap() == 8);

    auto none =
        IntResult::Err(std::string("e")).ok().map([](int v) { return v * 2; });
    CHECK(none.is_none());
}

TEST_CASE("member ok/err return types")
{
    static_assert(
        std::same_as<decltype(std::declval<IntResult &>().ok()), res::Option<int>>);
    static_assert(
        std::same_as<decltype(std::declval<IntResult &&>().ok()), res::Option<int>>);
    static_assert(std::same_as<decltype(std::declval<IntResult &>().err()),
                               res::Option<std::string>>);
    static_assert(std::same_as<decltype(std::declval<IntResult &&>().err()),
                               res::Option<std::string>>);
}

TEST_CASE("member ok on a void Result yields Option<void>")
{
    CHECK(VoidResult::Ok().ok().is_some());
    CHECK(VoidResult::Err(std::string("e")).ok().is_none());
}

TEST_CASE("member err on a void Result yields Option<E>")
{
    auto some = VoidResult::Err(std::string("e")).err();
    CHECK(some.is_some());
    CHECK(some.unwrap() == "e");
    CHECK(VoidResult::Ok().err().is_none());
}

TEST_CASE("member ok on a const void Result leaves source unchanged")
{
    const VoidResult v = VoidResult::Ok();
    CHECK(v.ok().is_some());
    CHECK(v.is_ok());
}

TEST_CASE("member ok on a moved void Result throws")
{
    using bad_access = res::bad_result_access;
    VoidResult v = VoidResult::Ok();
    auto o = std::move(v).ok();
    CHECK(o.is_some());
    CHECK(v.is_moved());
    CHECK_THROWS_AS((void)v.ok(), bad_access);
    CHECK_THROWS_AS((void)v.err(), bad_access);
}

TEST_CASE("free ok/err throw on a moved Result")
{
    using bad_access = res::bad_result_access;

    IntResult r = IntResult::Ok(7);
    CHECK(std::move(r).unwrap() == 7);
    CHECK(r.is_moved());
    CHECK_THROWS_AS((void)res::ok(r), bad_access);
    CHECK_THROWS_AS((void)res::err(r), bad_access);
    CHECK_THROWS_AS((void)res::ok(std::move(r)), bad_access);
    CHECK_THROWS_AS((void)res::err(std::move(r)), bad_access);
}

TEST_CASE("free ok/err on a moved void Result throw")
{
    using bad_access = res::bad_result_access;

    VoidResult v = VoidResult::Ok();
    auto o = std::move(v).ok();
    CHECK(o.is_some());
    CHECK(v.is_moved());
    CHECK_THROWS_AS((void)res::ok(v), bad_access);
    CHECK_THROWS_AS((void)res::err(v), bad_access);
}

TEST_CASE("free ok/err mirror the member results")
{
    IntResult okr = IntResult::Ok(5);
    CHECK(res::ok(okr) == okr.ok());

    IntResult errr = IntResult::Err(std::string("boom"));
    CHECK(res::err(errr) == errr.err());

    CHECK(res::ok(IntResult::Ok(5)) == IntResult::Ok(5).ok());
    CHECK(res::err(IntResult::Err(std::string("boom"))) ==
          IntResult::Err(std::string("boom")).err());
}
