#include <doctest/doctest.h>

#include <utility>

#include "pjh_result/option.hpp"
#include "pjh_result/result.hpp"
#include "support/instrumented.hpp"

namespace res = pjh::result;
using bad_access = pjh::result::bad_result_access;
using pjh_test::InstanceCounter;
using pjh_test::ThrowOnCopy;

TEST_CASE("no leak on scope exit")
{
    InstanceCounter::reset();
    {
        auto r = res::Result<InstanceCounter, int>::Ok(InstanceCounter{1});
        CHECK(r.is_ok());
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through copy")
{
    InstanceCounter::reset();
    {
        auto a = res::Result<InstanceCounter, int>::Ok(InstanceCounter{2});
        auto b = a;
        CHECK(b.unwrap().id == 2);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through move")
{
    InstanceCounter::reset();
    {
        auto a = res::Result<InstanceCounter, int>::Ok(InstanceCounter{3});
        auto b = std::move(a);
        CHECK(b.unwrap().id == 3);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through copy assignment")
{
    InstanceCounter::reset();
    {
        auto a = res::Result<InstanceCounter, int>::Ok(InstanceCounter{4});
        auto b = res::Result<InstanceCounter, int>::Ok(InstanceCounter{5});
        b = a;
        CHECK(b.unwrap().id == 4);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through move assignment")
{
    InstanceCounter::reset();
    {
        auto a = res::Result<InstanceCounter, int>::Ok(InstanceCounter{6});
        auto b = res::Result<InstanceCounter, int>::Ok(InstanceCounter{7});
        b = std::move(a);
        CHECK(b.unwrap().id == 6);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("copy assignment offers strong exception guarantee")
{
    auto a = res::Result<ThrowOnCopy, int>::Ok(ThrowOnCopy{9});
    auto b = res::Result<ThrowOnCopy, int>::Err(7);

    CHECK_THROWS(b = a); // 拷贝 a 的 ThrowOnCopy 时抛出
    CHECK(b.is_err());   // this 保持原状
    CHECK(b.unwrap_err() == 7);
}

TEST_CASE("no leak through rvalue unwrap")
{
    InstanceCounter::reset();
    {
        auto r = res::Result<InstanceCounter, int>::Ok(InstanceCounter{10});
        InstanceCounter v = std::move(r).unwrap();
        CHECK(v.id == 10);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through rvalue unwrap_err")
{
    InstanceCounter::reset();
    {
        auto r = res::Result<int, InstanceCounter>::Err(InstanceCounter{11});
        InstanceCounter e = std::move(r).unwrap_err();
        CHECK(e.id == 11);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through rvalue flatten of a nested Ok")
{
    InstanceCounter::reset();
    {
        auto outer = res::Result<res::Result<InstanceCounter, int>, int>::Ok(
            res::Result<InstanceCounter, int>::Ok(InstanceCounter{22}));
        auto flat = std::move(outer).flatten();
        CHECK(flat.unwrap().id == 22);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through rvalue flatten of a nested Err")
{
    InstanceCounter::reset();
    {
        auto outer =
            res::Result<res::Result<int, InstanceCounter>, InstanceCounter>::Ok(
                res::Result<int, InstanceCounter>::Err(InstanceCounter{21}));
        auto flat = std::move(outer).flatten();
        CHECK(flat.unwrap_err().id == 21);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through rvalue flatten of an outer Err")
{
    InstanceCounter::reset();
    {
        auto outer =
            res::Result<res::Result<int, InstanceCounter>, InstanceCounter>::Err(
                InstanceCounter{23});
        auto flat = std::move(outer).flatten();
        CHECK(flat.unwrap_err().id == 23);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through rvalue transpose of Some")
{
    InstanceCounter::reset();
    {
        auto outer = res::Result<res::Option<InstanceCounter>, int>::Ok(
            res::Option<InstanceCounter>::Some(InstanceCounter{31}));
        auto transposed = std::move(outer).transpose();
        REQUIRE(transposed.is_some());
        CHECK(transposed.unwrap().unwrap().id == 31);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through rvalue transpose of None")
{
    InstanceCounter::reset();
    {
        auto outer = res::Result<res::Option<InstanceCounter>, int>::Ok(
            res::Option<InstanceCounter>::None());
        auto transposed = std::move(outer).transpose();
        CHECK(transposed.is_none());
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("no leak through rvalue transpose of an outer Err")
{
    InstanceCounter::reset();
    {
        auto outer =
            res::Result<res::Option<int>, InstanceCounter>::Err(InstanceCounter{32});
        auto transposed = std::move(outer).transpose();
        REQUIRE(transposed.is_some());
        CHECK(transposed.unwrap().unwrap_err().id == 32);
    }
    CHECK(InstanceCounter::live == 0);
}

TEST_CASE("move construction from a Moved result preserves Moved")
{
    auto r = res::Result<InstanceCounter, int>::Ok(InstanceCounter{41});
    InstanceCounter v = std::move(r).unwrap();
    CHECK(v.id == 41);
    REQUIRE(r.is_moved());

    auto m = std::move(r);
    CHECK(m.is_moved());
    CHECK_FALSE(m.is_ok());
    CHECK_FALSE(m.is_err());
}

TEST_CASE("move assignment from a Moved result marks the target Moved")
{
    auto r = res::Result<int, std::string>::Ok(1);
    (void)std::move(r).unwrap();
    REQUIRE(r.is_moved());

    auto target = res::Result<int, std::string>::Err(std::string("old"));
    target = std::move(r);
    CHECK(target.is_moved());
}

TEST_CASE("copy construction from a Moved result throws")
{
    auto r = res::Result<int, std::string>::Ok(1);
    (void)std::move(r).unwrap();
    REQUIRE(r.is_moved());

    auto copy_r = [&] {
        auto c = r;
        (void)c;
    };
    CHECK_THROWS_AS(copy_r(), bad_access);
}

TEST_CASE("copy assignment from a Moved result throws and keeps the target")
{
    auto r = res::Result<int, std::string>::Ok(1);
    (void)std::move(r).unwrap();
    REQUIRE(r.is_moved());

    auto target = res::Result<int, std::string>::Ok(99);
    CHECK_THROWS_AS(target = r, bad_access);
    REQUIRE(target.is_ok());
    CHECK(target.unwrap() == 99);
}
