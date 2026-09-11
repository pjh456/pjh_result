#include <doctest/doctest.h>

#include <string_view>

#include "pjh_result.hpp"

namespace res = pjh::result;

namespace
{
    struct GoodErr
    {
        enum class Kind
        {
            io,
            parse
        };

        std::string_view message() const { return "io"; }
        Kind kind() const { return Kind::io; }
    };

    // Has message() but no kind().
    struct MessageOnly
    {
        std::string_view message() const { return "m"; }
    };

    // kind() exists but returns void, so it is not equality-comparable.
    struct VoidKind
    {
        std::string_view message() const { return "m"; }
        void kind() const {}
    };

    static_assert(res::Diagnostic<GoodErr>);
    static_assert(!res::Diagnostic<MessageOnly>);
    static_assert(!res::Diagnostic<VoidKind>);
    static_assert(res::Diagnostic<res::Context<GoodErr>>);
    static_assert(!res::Diagnostic<res::Context<MessageOnly>>);
    static_assert(!res::Diagnostic<res::Context<VoidKind>>);

    // Result + context macros integration helpers.
    res::Result<int, res::Context<GoodErr>> read(int v)
    {
        if (v < 0)
            return res::Result<int, res::Context<GoodErr>>::Err(
                res::Context<GoodErr>(GoodErr{}).context("read"));
        return res::Result<int, res::Context<GoodErr>>::Ok(v);
    }

    res::Result<int, res::Context<GoodErr>> load(int v)
    {
        ASSIGN_OR_RETURN_CTX(x, read(v), "load");
        return res::Result<int, res::Context<GoodErr>>::Ok(x + 1);
    }

    res::Result<void, res::Context<GoodErr>> verify(int v)
    {
        if (v < 0)
            return res::Result<void, res::Context<GoodErr>>::Err(res::Context<GoodErr>(GoodErr{}));
        return res::Result<void, res::Context<GoodErr>>::Ok();
    }

    res::Result<void, res::Context<GoodErr>> verify_twice(int v)
    {
        TRY_CTX(verify(v), "verify");
        return res::Result<void, res::Context<GoodErr>>::Ok();
    }
}

TEST_CASE("plain Diagnostic renders just its message")
{
    CHECK(res::render(GoodErr{}) == "io");
}

TEST_CASE("single-layer Context renders outer then root")
{
    auto r = res::Result<int, GoodErr>::Err(GoodErr{}).context("load");

    REQUIRE(r.is_err());
    CHECK(res::render(r.unwrap_err()) == "load: io");
}

TEST_CASE("multi-layer Context renders outermost-first")
{
    auto r = res::Result<int, GoodErr>::Err(GoodErr{})
                 .context("parse json")
                 .context("load deck");

    REQUIRE(r.is_err());
    CHECK(res::render(r.unwrap_err()) == "load deck: parse json: io");
}

TEST_CASE("explicitly nested Context recurses with the same ordering")
{
    res::Context<res::Context<GoodErr>> nested(
        res::Context<GoodErr>(GoodErr{}).context("inner"));

    CHECK(res::render(nested) == "inner: io");
    CHECK(res::render(nested.context("outer")) == "outer: inner: io");
}

TEST_CASE("Context with an empty chain renders only the root message")
{
    res::Context<GoodErr> c(GoodErr{});

    CHECK(c.message().empty());
    CHECK(res::render(c) == "io");
}

TEST_CASE("Context::message returns the outermost layer and kind forwards the root")
{
    auto c = res::Context<GoodErr>(GoodErr{}).context("inner").context("outer");

    CHECK(c.message() == "outer");
    CHECK(c.kind() == GoodErr::Kind::io);
    CHECK(c.kind() == c.root_cause().kind());
}

TEST_CASE("Diagnostic rendering composes with Result and the context macros")
{
    auto ok = load(4);
    REQUIRE(ok.is_ok());
    CHECK(ok.unwrap() == 5);

    auto err = load(-1);
    REQUIRE(err.is_err());
    CHECK(res::render(err.unwrap_err()) == "load: read: io");

    auto vok = verify_twice(3);
    CHECK(vok.is_ok());

    auto verr = verify_twice(-1);
    REQUIRE(verr.is_err());
    CHECK(res::render(verr.unwrap_err()) == "verify: io");
}
