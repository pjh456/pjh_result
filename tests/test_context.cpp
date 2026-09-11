#include <doctest/doctest.h>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "pjh_result.hpp"

namespace res = pjh::result;

namespace
{
    enum class Kind
    {
        io,
        parse
    };

    struct Err
    {
        Kind kind;
        friend bool operator==(const Err &, const Err &) = default;
    };

    // Macro-based helpers: verify TRY / ASSIGN_OR_RETURN keep working with a Context error.
    res::Result<int, Err> parse_int(int v)
    {
        if (v < 0)
            return res::Result<int, Err>::Err(Err{Kind::parse});
        return res::Result<int, Err>::Ok(v);
    }

    res::Result<int, res::Context<Err>> parse_int_ctx(int v)
    {
        if (v < 0)
            return res::Result<int, res::Context<Err>>::Err(res::Context<Err>(Err{Kind::parse}).context("parse"));
        return res::Result<int, res::Context<Err>>::Ok(v);
    }

    res::Result<int, res::Context<Err>> parse_and_bump(int v)
    {
        ASSIGN_OR_RETURN(x, parse_int_ctx(v));
        return res::Result<int, res::Context<Err>>::Ok(x + 1);
    }

    res::Result<void, Err> check_positive(int v)
    {
        if (v < 0)
            return res::Result<void, Err>::Err(Err{Kind::io});
        return res::Result<void, Err>::Ok();
    }

    res::Result<void, res::Context<Err>> check_positive_ctx(int v)
    {
        TRY(check_positive(v).context("check"));
        return res::Result<void, res::Context<Err>>::Ok();
    }
}

// 编译期：messages() 的右值重载只移动 vector，必须与 const& 重载一样标为 noexcept
static_assert(noexcept(std::declval<res::Context<Err> &&>().messages()));
static_assert(noexcept(std::declval<const res::Context<Err> &>().messages()));

namespace
{
    template <typename C>
    concept RvalueRootCause = requires(C t) { std::move(t).root_cause(); };

    template <typename C>
    concept ConstRvalueRootCause = requires(const C &t) { std::move(t).root_cause(); };

    template <typename C>
    concept RvalueMessages = requires(C t) { std::move(t).messages(); };

    template <typename C>
    concept ConstRvalueMessages = requires(const C &t) { std::move(t).messages(); };
}

// 编译期：引用返回访问器的 const 右值形态被 delete 拒绝，避免临时量悬垂
static_assert(RvalueRootCause<res::Context<Err>>);
static_assert(RvalueMessages<res::Context<Err>>);
static_assert(!ConstRvalueRootCause<res::Context<Err>>);
static_assert(!ConstRvalueMessages<res::Context<Err>>);
static_assert(requires(const res::Context<Err> &c) {
    c.root_cause();
    c.messages();
});

TEST_CASE("context on Ok passes the value through without constructing a context")
{
    auto r = res::Result<int, Err>::Ok(42);
    auto c = r.context("load");

    static_assert(std::is_same_v<decltype(c), res::Result<int, res::Context<Err>>>);
    CHECK(c.is_ok());
    CHECK(c.unwrap() == 42);
}

TEST_CASE("context on void Ok stays Ok")
{
    auto r = res::Result<void, Err>::Ok();
    auto c = r.context("should not matter");

    static_assert(std::is_same_v<decltype(c), res::Result<void, res::Context<Err>>>);
    CHECK(c.is_ok());
}

TEST_CASE("with_context is never invoked on Ok")
{
    int calls = 0;
    auto r = res::Result<int, Err>::Ok(7);
    auto c = r.with_context(
        [&]
        {
            ++calls;
            return "load";
        });

    CHECK(calls == 0);
    CHECK(c.is_ok());
    CHECK(c.unwrap() == 7);
}

TEST_CASE("single-layer context records the message and the root cause")
{
    auto r = res::Result<int, Err>::Err(Err{Kind::io}).context("reading file");

    REQUIRE(r.is_err());
    const auto &ctx = r.unwrap_err();
    CHECK(ctx.root_cause().kind == Kind::io);
    REQUIRE(ctx.messages().size() == 1);
    CHECK(ctx.messages()[0] == "reading file");
}

TEST_CASE("multi-layer context is outermost-first and stable")
{
    auto r = res::Result<int, Err>::Err(Err{Kind::parse})
                 .context("parse json")
                 .context("load deck");

    static_assert(std::is_same_v<decltype(r), res::Result<int, res::Context<Err>>>);
    REQUIRE(r.is_err());
    const auto &m = r.unwrap_err().messages();
    REQUIRE(m.size() == 2);
    CHECK(m[0] == "load deck");
    CHECK(m[1] == "parse json");
    CHECK(r.unwrap_err().root_cause().kind == Kind::parse);
}

TEST_CASE("repeated context on an already-Context error stays one layer deep")
{
    auto r = parse_int_ctx(-1).context("a").context("b");

    static_assert(std::is_same_v<decltype(r), res::Result<int, res::Context<Err>>>);
    REQUIRE(r.is_err());
    const auto &m = r.unwrap_err().messages();
    REQUIRE(m.size() == 3);
    CHECK(m[0] == "b");
    CHECK(m[1] == "a");
    CHECK(m[2] == "parse");
}

TEST_CASE("with_context appends on Err and invokes the producer once per layer")
{
    int calls = 0;
    auto r = res::Result<int, Err>::Err(Err{Kind::io})
                 .with_context(
                     [&]
                     {
                         ++calls;
                         return std::string("read file");
                     })
                 .with_context(
                     [&]
                     {
                         ++calls;
                         return "load deck";
                     });

    CHECK(calls == 2);
    REQUIRE(r.is_err());
    const auto &m = r.unwrap_err().messages();
    REQUIRE(m.size() == 2);
    CHECK(m[0] == "load deck");
    CHECK(m[1] == "read file");
}

TEST_CASE("root_cause preserves the original error type")
{
    auto r = res::Result<int, Err>::Err(Err{Kind::parse}).context("x");

    auto &ctx = r.unwrap_err();
    static_assert(std::is_same_v<decltype(ctx.root_cause()), const Err &>);
    CHECK(ctx.root_cause() == Err{Kind::parse});

    Err moved = std::move(ctx).root_cause();
    CHECK(moved.kind == Kind::parse);
}

TEST_CASE("Context itself supports context and with_context")
{
    res::Context<Err> c(Err{Kind::io});
    auto c2 = c.context("inner").context("outer");
    REQUIRE(c2.messages().size() == 2);
    CHECK(c2.messages()[0] == "outer");
    CHECK(c2.messages()[1] == "inner");
    CHECK(c2.root_cause().kind == Kind::io);

    int calls = 0;
    auto c3 = c.with_context(
        [&]
        {
            ++calls;
            return "lazy";
        });
    CHECK(calls == 1);
    REQUIRE(c3.messages().size() == 1);
    CHECK(c3.messages()[0] == "lazy");
}

TEST_CASE("Context iterates messages outermost-first")
{
    auto c = res::Result<void, Err>::Err(Err{Kind::io}).context("a").context("b");

    std::vector<std::string> got;
    for (const auto &s : c.unwrap_err())
        got.push_back(s);

    REQUIRE(got.size() == 2);
    CHECK(got[0] == "b");
    CHECK(got[1] == "a");
}

TEST_CASE("context composes with map_err")
{
    auto r = res::Result<int, Err>::Err(Err{Kind::io})
                 .map_err(
                     [](const Err &e)
                     {
                         return e.kind == Kind::io ? std::string("io") : std::string("parse");
                     })
                 .context("load");

    static_assert(std::is_same_v<decltype(r), res::Result<int, res::Context<std::string>>>);
    REQUIRE(r.is_err());
    CHECK(r.unwrap_err().root_cause() == "io");
    CHECK(r.unwrap_err().messages()[0] == "load");
}

TEST_CASE("context composes with and_then and or_else")
{
    auto chained = res::Result<int, res::Context<Err>>::Err(res::Context<Err>(Err{Kind::parse}).context("parse"))
                       .and_then(
                           [](int v) -> res::Result<int, res::Context<Err>>
                           {
                               return res::Result<int, res::Context<Err>>::Ok(v + 1);
                           });
    REQUIRE(chained.is_err());
    CHECK(chained.unwrap_err().messages()[0] == "parse");

    auto recovered = res::Result<int, res::Context<Err>>::Err(res::Context<Err>(Err{Kind::io}).context("io"))
                         .or_else(
                             [](const res::Context<Err> &)
                             {
                                 return res::Result<int, res::Context<Err>>::Ok(99);
                             });
    REQUIRE(recovered.is_ok());
    CHECK(recovered.unwrap() == 99);
}

TEST_CASE("const& combinators copy a Context error")
{
    const auto src = res::Result<int, res::Context<Err>>::Err(res::Context<Err>(Err{Kind::io}).context("a"));

    auto mapped = src.map([](int v) { return v + 1; });
    static_assert(std::is_same_v<decltype(mapped), res::Result<int, res::Context<Err>>>);
    REQUIRE(mapped.is_err());
    CHECK(mapped.unwrap_err().messages()[0] == "a");

    const auto src2 = res::Result<int, res::Context<Err>>::Err(res::Context<Err>(Err{Kind::parse}).context("b"));
    auto chained = src2.and_then(
        [](int v) -> res::Result<int, res::Context<Err>>
        {
            return res::Result<int, res::Context<Err>>::Ok(v);
        });
    REQUIRE(chained.is_err());
    CHECK(chained.unwrap_err().messages()[0] == "b");
}

TEST_CASE("ASSIGN_OR_RETURN and TRY propagate Context errors")
{
    auto ok = parse_and_bump(4);
    REQUIRE(ok.is_ok());
    CHECK(ok.unwrap() == 5);

    auto err = parse_and_bump(-1);
    REQUIRE(err.is_err());
    REQUIRE(err.unwrap_err().messages().size() == 1);
    CHECK(err.unwrap_err().messages()[0] == "parse");

    auto cok = check_positive_ctx(3);
    CHECK(cok.is_ok());

    auto cerr = check_positive_ctx(-1);
    REQUIRE(cerr.is_err());
    CHECK(cerr.unwrap_err().messages()[0] == "check");
}

TEST_CASE("move-only error type works through the rvalue context path")
{
    struct MoveOnly
    {
        MoveOnly() = default;
        MoveOnly(MoveOnly &&) noexcept = default;
        MoveOnly &operator=(MoveOnly &&) noexcept = default;
        MoveOnly(const MoveOnly &) = delete;
        MoveOnly &operator=(const MoveOnly &) = delete;
        int code = 7;
    };

    auto r = res::Result<int, MoveOnly>::Err(MoveOnly{}).context("load");

    static_assert(std::is_same_v<decltype(r), res::Result<int, res::Context<MoveOnly>>>);
    REQUIRE(r.is_err());
    CHECK(std::move(r).unwrap_err().root_cause().code == 7);
}

TEST_CASE("Context move and copy traits follow the error type")
{
    static_assert(std::is_copy_constructible_v<res::Context<Err>>);
    static_assert(std::is_nothrow_move_constructible_v<res::Context<Err>>);

    struct ThrowMove
    {
        ThrowMove() = default;
        ThrowMove(ThrowMove &&) {}
        ThrowMove &operator=(ThrowMove &&) { return *this; }
    };
    static_assert(!std::is_nothrow_move_constructible_v<res::Context<ThrowMove>>);
}

TEST_CASE("plain Result error still works through map on the Err path")
{
    auto r = parse_int(-1).map([](int v) { return v + 1; });

    static_assert(std::is_same_v<decltype(r), res::Result<int, Err>>);
    REQUIRE(r.is_err());
    CHECK(r.unwrap_err() == Err{Kind::parse});
}
