#include <doctest/doctest.h>

#include <utility>

#include "pjh_result/result.hpp"
#include "support/instrumented.hpp"

namespace rp = pjh::result::utils;
using pjh_test::InstanceCounter;

TEST_CASE("no leak on scope exit")
{
    InstanceCounter::reset();
    {
        auto r = rp::Result<InstanceCounter, int>::Ok(InstanceCounter{1});
        CHECK(r.is_ok());
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through copy")
{
    InstanceCounter::reset();
    {
        auto a = rp::Result<InstanceCounter, int>::Ok(InstanceCounter{2});
        auto b = a;
        CHECK(b.unwrap().id == 2);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through move")
{
    InstanceCounter::reset();
    {
        auto a = rp::Result<InstanceCounter, int>::Ok(InstanceCounter{3});
        auto b = std::move(a);
        CHECK(b.unwrap().id == 3);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through copy assignment")
{
    InstanceCounter::reset();
    {
        auto a = rp::Result<InstanceCounter, int>::Ok(InstanceCounter{4});
        auto b = rp::Result<InstanceCounter, int>::Ok(InstanceCounter{5});
        b = a;
        CHECK(b.unwrap().id == 4);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through move assignment")
{
    InstanceCounter::reset();
    {
        auto a = rp::Result<InstanceCounter, int>::Ok(InstanceCounter{6});
        auto b = rp::Result<InstanceCounter, int>::Ok(InstanceCounter{7});
        b = std::move(a);
        CHECK(b.unwrap().id == 6);
    }
    CHECK(InstanceCounter::live == 0);
}
