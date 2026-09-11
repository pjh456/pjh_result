#include <doctest/doctest.h>

#include <string>
#include <type_traits>
#include <utility>

#include "pjh_result/context.hpp"
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

namespace
{
    using IntCtx = res::Result<int, res::Context<std::string>>;
    using VoidCtx = res::Result<void, res::Context<std::string>>;

    IntCtx use_assign_ctx(bool good)
    {
        ASSIGN_OR_RETURN_CTX(v, parse(good), "read number");
        return IntCtx::Ok(v + 1);
    }

    VoidCtx use_try_ctx(bool good)
    {
        TRY_CTX(parse(good), "read number");
        return VoidCtx::Ok();
    }

    VoidCtx lazy_ctx(bool good, int &calls)
    {
        TRY_CTX(parse(good), (++calls, std::string("read number")));
        return VoidCtx::Ok();
    }

    IntCtx inner_ctx(bool good)
    {
        ASSIGN_OR_RETURN_CTX(v, parse(good), "parse number");
        return IntCtx::Ok(v);
    }

    VoidCtx outer_ctx(bool good)
    {
        TRY_CTX(inner_ctx(good), "load save");
        return VoidCtx::Ok();
    }

    struct MoveOnlyErr
    {
        MoveOnlyErr() = default;
        MoveOnlyErr(MoveOnlyErr &&) noexcept = default;
        MoveOnlyErr &operator=(MoveOnlyErr &&) noexcept = default;
        MoveOnlyErr(const MoveOnlyErr &) = delete;
        MoveOnlyErr &operator=(const MoveOnlyErr &) = delete;
        int code = 7;
    };

    res::Result<int, MoveOnlyErr> make_move_only(bool good)
    {
        if (!good)
            return res::Result<int, MoveOnlyErr>::Err(MoveOnlyErr{});
        return res::Result<int, MoveOnlyErr>::Ok(3);
    }

    res::Result<int, res::Context<MoveOnlyErr>> move_only_ctx(bool good)
    {
        ASSIGN_OR_RETURN_CTX(v, make_move_only(good), "wrap move-only");
        return res::Result<int, res::Context<MoveOnlyErr>>::Ok(v + 1);
    }
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

TEST_CASE("ASSIGN_OR_RETURN_CTX unwraps on Ok, propagates with context on Err")
{
    static_assert(std::is_same_v<decltype(use_assign_ctx(true)), IntCtx>);
    CHECK(use_assign_ctx(true).unwrap() == 11);

    auto e = use_assign_ctx(false);
    REQUIRE(e.is_err());
    const auto &ctx = e.unwrap_err();
    REQUIRE(ctx.messages().size() == 1);
    CHECK(ctx.messages()[0] == "read number");
    CHECK(ctx.root_cause() == "bad");
}

TEST_CASE("TRY_CTX continues on Ok, propagates with context on Err")
{
    static_assert(std::is_same_v<decltype(use_try_ctx(true)), VoidCtx>);
    CHECK(use_try_ctx(true).is_ok());

    auto e = use_try_ctx(false);
    REQUIRE(e.is_err());
    const auto &ctx = e.unwrap_err();
    REQUIRE(ctx.messages().size() == 1);
    CHECK(ctx.messages()[0] == "read number");
    CHECK(ctx.root_cause() == "bad");
}

TEST_CASE("nested TRY_CTX accumulates context layers outermost-first")
{
    CHECK(outer_ctx(true).is_ok());

    auto e = outer_ctx(false);
    REQUIRE(e.is_err());
    const auto &ctx = e.unwrap_err();
    REQUIRE(ctx.messages().size() == 2);
    CHECK(ctx.messages()[0] == "load save");
    CHECK(ctx.messages()[1] == "parse number");
    CHECK(ctx.root_cause() == "bad");
}

TEST_CASE("context macro message is evaluated only on the Err path")
{
    int calls = 0;
    CHECK(lazy_ctx(true, calls).is_ok());
    CHECK(calls == 0);

    auto e = lazy_ctx(false, calls);
    REQUIRE(e.is_err());
    CHECK(calls == 1);
    CHECK(e.unwrap_err().messages()[0] == "read number");
}

TEST_CASE("context macros propagate a move-only error type")
{
    auto ok = move_only_ctx(true);
    REQUIRE(ok.is_ok());
    CHECK(ok.unwrap() == 4);

    auto err = move_only_ctx(false);
    REQUIRE(err.is_err());
    auto ctx = std::move(err).unwrap_err();
    CHECK(ctx.root_cause().code == 7);
    REQUIRE(ctx.messages().size() == 1);
    CHECK(ctx.messages()[0] == "wrap move-only");
}
