#include <doctest/doctest.h>

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include "pjh_result.hpp"
#include "pjh_result/format.hpp"
#include "pjh_result/io.hpp"

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

        // Satisfies Diagnostic only: no operator<<.
        struct DiagOnly
        {
            std::string_view message() const { return "oom"; }
            int kind() const { return 1; }
        };

        // Satisfies Diagnostic AND has a user operator<<, which must win.
        struct StreamedErr
        {
            std::string_view message() const { return "raw"; }
            int kind() const { return 3; }
        };

        inline std::ostream &operator<<(std::ostream &os, const StreamedErr &)
        {
            return os << "CUSTOM";
        }
    }
}

using RI = res::Result<int, long>;

TEST_CASE("operator<< renders Result Ok Err and void Ok")
{
    {
        std::ostringstream os;
        os << RI::Ok(3);
        CHECK(os.str() == "Ok(3)");
    }
    {
        std::ostringstream os;
        os << RI::Err(7);
        CHECK(os.str() == "Err(7)");
    }
    {
        std::ostringstream os;
        os << res::Result<void, int>::Ok();
        CHECK(os.str() == "Ok()");
    }
    {
        std::ostringstream os;
        os << res::Result<void, int>::Err(7);
        CHECK(os.str() == "Err(7)");
    }
}

TEST_CASE("operator<< renders a moved-from result without throwing")
{
    auto r = RI::Ok(1);
    (void)std::move(r).unwrap();

    std::ostringstream os;
    CHECK_NOTHROW(os << r);
    CHECK(os.str() == "Result(moved)");
}

TEST_CASE("operator<< renders Option Some None and void")
{
    {
        std::ostringstream os;
        os << res::Option<int>::Some(3);
        CHECK(os.str() == "Some(3)");
    }
    {
        std::ostringstream os;
        os << res::Option<int>::None();
        CHECK(os.str() == "None");
    }
    {
        std::ostringstream os;
        os << res::Option<void>::Some();
        CHECK(os.str() == "Some()");
    }
    {
        std::ostringstream os;
        os << res::Option<void>::None();
        CHECK(os.str() == "None");
    }
}

TEST_CASE("operator<< falls back to render for a Diagnostic-only error")
{
    std::ostringstream os;
    os << res::Result<int, pjh_test::DiagOnly>::Err(pjh_test::DiagOnly{});
    CHECK(os.str() == "Err(oom)");
}

TEST_CASE("operator<< prefers a user operator<< over render")
{
    std::ostringstream os;
    os << res::Result<int, pjh_test::StreamedErr>::Err(pjh_test::StreamedErr{});
    CHECK(os.str() == "Err(CUSTOM)");
}

TEST_CASE("operator<< renders the full Context chain")
{
    using Ctx = res::Context<pjh_test::GoodErr>;
    auto c = Ctx(pjh_test::GoodErr{}).context("a").context("b");

    std::ostringstream os;
    os << c;
    CHECK(os.str() == "b: a: io");
}

TEST_CASE("operator<< applies setw to the whole representation")
{
    std::ostringstream os;
    os << std::setw(10) << RI::Ok(3);
    CHECK(os.str() == "     Ok(3)");
}

#ifdef PJH_RESULT_HAS_STD_FORMAT
TEST_CASE("operator<< and std::formatter agree for scalar elements")
{
    auto r = RI::Ok(3);
    auto e = RI::Err(7);

    std::ostringstream os_r;
    os_r << r;
    std::ostringstream os_e;
    os_e << e;

    CHECK(os_r.str() == std::format("{}", r));
    CHECK(os_e.str() == std::format("{}", e));
}
#endif // PJH_RESULT_HAS_STD_FORMAT
