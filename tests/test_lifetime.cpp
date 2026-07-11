#include <doctest/doctest.h>

#include <utility>

#include "pjh_result/result.hpp"
#include "support/instrumented.hpp"

namespace res = pjh::result;
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
