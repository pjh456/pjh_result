#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <type_traits>

#include "pjh_result.hpp"
#include "pjh_result/format.hpp"

namespace res = pjh::result;

namespace pjh_test
{
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

        // Satisfies Diagnostic only: no std::formatter.
        struct DiagOnly
        {
            std::string_view message() const { return "oom"; }
            int kind() const { return 1; }
        };

        // Satisfies Diagnostic AND has a user formatter, which must win.
        struct DiagFormatted
        {
            std::string_view message() const { return "io"; }
            int kind() const { return 2; }
        };

        // Neither formattable nor Diagnostic.
        struct NotFormattable
        {
        };
    }
}

#ifdef PJH_RESULT_HAS_STD_FORMAT

namespace std
{
    template <>
    struct formatter<pjh_test::DiagFormatted, char> : formatter<string_view, char>
    {
        template <class Ctx>
        auto format(const pjh_test::DiagFormatted &, Ctx &ctx) const
        {
            return formatter<string_view, char>::format("CUSTOM", ctx);
        }
    };
}

using RI = res::Result<int, long>;

static_assert(res::detail::formattable<RI>);
static_assert(res::detail::formattable<res::Option<int>>);
static_assert(res::detail::formattable<res::Result<int, pjh_test::DiagOnly>>);
static_assert(res::detail::formattable<res::Context<pjh_test::GoodErr>>);
static_assert(!res::detail::formattable<pjh_test::NotFormattable>);
static_assert(!res::detail::formattable<res::Result<int, pjh_test::NotFormattable>>);
static_assert(!res::detail::formattable<res::Option<pjh_test::NotFormattable>>);
static_assert(std::is_same_v<decltype(std::format("{}", RI::Ok(3))), std::string>);

TEST_CASE("format Result renders Ok Err and void Ok")
{
    CHECK(std::format("{}", RI::Ok(3)) == "Ok(3)");
    CHECK(std::format("{}", RI::Err(7)) == "Err(7)");
    CHECK(std::format("{}", res::Result<void, int>::Ok()) == "Ok()");
    CHECK(std::format("{}", res::Result<void, int>::Err(7)) == "Err(7)");
}

TEST_CASE("format Result renders a moved-from result without throwing")
{
    auto r = RI::Ok(1);
    (void)std::move(r).unwrap();

    REQUIRE(r.is_moved());
    std::string s;
    CHECK_NOTHROW(s = std::format("{}", r));
    CHECK(s == "Result(moved)");
}

TEST_CASE("format Result preserves a string element via its own formatter")
{
    using RS = res::Result<int, std::string>;
    CHECK(std::format("{}", RS::Err(std::string("e"))) == "Err(e)");
}

TEST_CASE("format Option renders Some None and void")
{
    CHECK(std::format("{}", res::Option<int>::Some(3)) == "Some(3)");
    CHECK(std::format("{}", res::Option<int>::None()) == "None");
    CHECK(std::format("{}", res::Option<void>::Some()) == "Some()");
    CHECK(std::format("{}", res::Option<void>::None()) == "None");
}

TEST_CASE("format nests Option inside Result")
{
    using RN = res::Result<res::Option<int>, int>;
    CHECK(std::format("{}", RN::Ok(res::Option<int>::Some(3))) == "Ok(Some(3))");
    CHECK(std::format("{}", RN::Ok(res::Option<int>::None())) == "Ok(None)");
}

TEST_CASE("format Result falls back to render for a Diagnostic-only error")
{
    using RD = res::Result<int, pjh_test::DiagOnly>;
    CHECK(std::format("{}", RD::Err(pjh_test::DiagOnly{})) == "Err(oom)");
}

TEST_CASE("format Result prefers a user std::formatter over render")
{
    using RF = res::Result<int, pjh_test::DiagFormatted>;
    CHECK(std::format("{}", RF::Err(pjh_test::DiagFormatted{})) == "Err(CUSTOM)");
}

TEST_CASE("format Context renders the full outer-to-inner causal chain")
{
    using Ctx = res::Context<pjh_test::GoodErr>;
    auto c = Ctx(pjh_test::GoodErr{}).context("a").context("b");
    CHECK(std::format("{}", c) == "b: a: io");

    using RC = res::Result<int, Ctx>;
    auto r = RC::Err(Ctx(pjh_test::GoodErr{}).context("a").context("b"));
    CHECK(std::format("{}", r) == "Err(b: a: io)");
}

TEST_CASE("format passes the spec through to the whole representation")
{
    CHECK(std::format("{:>10}", RI::Ok(3)) == "     Ok(3)");
    CHECK(std::format("{:*<8}", RI::Ok(3)) == "Ok(3)***");
}

TEST_CASE("vformat uses the same formatter specializations")
{
    auto r = RI::Ok(3);
    CHECK(std::vformat("{}", std::make_format_args(r)) == "Ok(3)");
}

#else

TEST_CASE("format header degrades gracefully without <format>")
{
    CHECK(true);
}

#endif // PJH_RESULT_HAS_STD_FORMAT
