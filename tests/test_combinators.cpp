#include <doctest/doctest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "pjh_result/result.hpp"

namespace res = pjh::result;
using bad_access = pjh::result::bad_result_access;

using StrResult = res::Result<int, std::string>;

namespace
{
    void observe_ok(int) {}
    void observe_err(const std::string &) {}

    StrResult make_ok(int v)
    {
        return StrResult::Ok(v);
    }
}

// 编译期：右值调用按值返回，左值调用仍返回 const 引用
static_assert(std::is_same_v<
              decltype(std::declval<StrResult>().inspect(&observe_ok)),
              StrResult>);
static_assert(!std::is_reference_v<
              decltype(std::declval<StrResult>().inspect(&observe_ok))>);
static_assert(std::is_same_v<
              decltype(std::declval<StrResult>().inspect_err(&observe_err)),
              StrResult>);
static_assert(!std::is_reference_v<
              decltype(std::declval<StrResult>().inspect_err(&observe_err))>);
static_assert(std::is_same_v<
              decltype(std::declval<StrResult &>().inspect(&observe_ok)),
              const StrResult &>);
static_assert(std::is_same_v<
              decltype(std::declval<StrResult &>().inspect_err(&observe_err)),
              const StrResult &>);

TEST_CASE("map transforms the Ok value")
{
    auto r = res::Result<int, std::string>::Ok(10).map(
        [](int v)
        { return v * 2; });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 20);
}

TEST_CASE("map passes through Err untouched")
{
    auto r = res::Result<int, std::string>::Err(std::string("e"))
                 .map(
                     [](int v)
                     { return v * 2; });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "e");
}

TEST_CASE("map returning void yields Result<void, E>")
{
    bool called = false;
    auto r = res::Result<int, std::string>::Ok(3).map(
        [&](int)
        { called = true; });
    CHECK(called);
    CHECK(r.is_ok());
}

TEST_CASE("map_err transforms the error")
{
    auto r = res::Result<int, std::string>::Err(std::string("e"))
                 .map_err([](const std::string &s)
                          { return s.size(); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == std::size_t{1});
}

TEST_CASE("map_err passes through Ok untouched")
{
    auto r = res::Result<int, std::string>::Ok(5)
                 .map_err([](const std::string &s)
                          { return s.size(); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 5);
}

TEST_CASE("and_then chains on Ok")
{
    auto r = res::Result<int, std::string>::Ok(3).and_then(
        [](int x)
        { return res::Result<int, std::string>::Ok(x + 100); });
    CHECK(r.unwrap() == 103);
}

TEST_CASE("and_then short-circuits on Err")
{
    auto r = res::Result<int, std::string>::Err(std::string("e")).and_then([](int x)
                                                                           { return res::Result<int, std::string>::Ok(x + 100); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "e");
}

TEST_CASE("or_else recovers from Err")
{
    auto r = res::Result<int, std::string>::Err(std::string("e"))
                 .or_else([](const std::string &)
                          { return res::Result<int, std::string>::Ok(0); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 0);
}

TEST_CASE("or_else passes Ok through unchanged")
{
    auto r = res::Result<int, std::string>::Ok(7).or_else(
        [](const std::string &)
        { return res::Result<int, std::string>::Ok(0); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 7);
}

TEST_CASE("or_else can change the error type")
{
    auto r = res::Result<int, std::string>::Err(std::string("e"))
                 .or_else([](const std::string &e)
                          { return res::Result<int, std::size_t>::Err(e.size()); });
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == std::size_t{1});
}

TEST_CASE("map_or returns transform on Ok, default on Err")
{
    auto ok = res::Result<int, std::string>::Ok(10);
    CHECK(ok.map_or(-1, [](int v)
                    { return v * 2; }) == 20);

    auto err = res::Result<int, std::string>::Err(std::string("e"));
    CHECK(err.map_or(-1, [](int v)
                     { return v * 2; }) == -1);
}

TEST_CASE("map_or_else picks transform or error fallback")
{
    auto ok = res::Result<int, std::string>::Ok(10);
    CHECK(ok.map_or_else(
              [](const std::string &)
              { return -1; }, [](int v)
              { return v * 2; }) == 20);

    auto err = res::Result<int, std::string>::Err(std::string("abc"));
    CHECK(err.map_or_else(
              [](const std::string &e)
              { return static_cast<int>(e.size()); },
              [](int v)
              { return v * 2; }) == 3);
}

TEST_CASE("inspect observes Ok value and returns self")
{
    int seen = 0;
    auto r = res::Result<int, std::string>::Ok(7);
    const auto &same = r.inspect(
        [&](int v)
        { seen = v; });
    CHECK(seen == 7);
    CHECK(&same == &r);

    seen = 0;
    auto e = res::Result<int, std::string>::Err(std::string("e"));
    e.inspect([&](int v)
              { seen = v; });
    CHECK(seen == 0); // not invoked on Err
}

TEST_CASE("inspect_err observes Err value and returns self")
{
    std::string seen;
    auto e = res::Result<int, std::string>::Err(std::string("boom"));
    e.inspect_err(
        [&](const std::string &v)
        { seen = v; });
    CHECK(seen == "boom");

    seen.clear();
    auto ok = res::Result<int, std::string>::Ok(1);
    ok.inspect_err(
        [&](const std::string &v)
        { seen = v; });
    CHECK(seen.empty()); // not invoked on Ok
}

TEST_CASE("inspect_err accepts callable taking E & (non-const lvalue ref)")
{
    auto e = res::Result<int, std::string>::Err(std::string("boom"));
    std::size_t n = 0;
    e.inspect_err(
        [&](const std::string &v)
        { n = v.size(); });
    CHECK(n == 4);
}

TEST_CASE("map throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    std::move(r).unwrap();
    CHECK_THROWS_AS(
        r.map(
            [](int v)
            { return v * 2; }),
        bad_access);
}

TEST_CASE("map_err throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    std::move(r).unwrap();
    CHECK_THROWS_AS(
        r.map_err(
            [](const std::string &e)
            { return e.size(); }),
        bad_access);
}

TEST_CASE("map_or_else throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    std::move(r).unwrap();
    CHECK_THROWS_AS(
        r.map_or_else(
            [](const std::string &e)
            { return static_cast<int>(e.size()); },
            [](int v)
            { return v * 2; }),
        bad_access);
}

TEST_CASE("and_then throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    std::move(r).unwrap();
    CHECK_THROWS_AS(
        r.and_then(
            [](int x)
            { return res::Result<int, std::string>::Ok(x + 1); }),
        bad_access);
}

TEST_CASE("or_else throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    std::move(r).unwrap();
    CHECK_THROWS_AS(
        r.or_else(
            [](const std::string &)
            { return res::Result<int, std::string>::Ok(0); }),
        bad_access);
}

TEST_CASE("unwrap_or_else throws on moved Result")
{
    auto r = res::Result<int, std::string>::Ok(5);
    std::move(r).unwrap();
    CHECK_THROWS_AS(
        r.unwrap_or_else(
            [](const std::string &)
            { return -1; }),
        bad_access);
}

TEST_CASE("operator== throws on moved Result")
{
    auto a = res::Result<int, std::string>::Ok(1);
    auto b = res::Result<int, std::string>::Ok(2);
    std::move(a).unwrap();
    std::move(b).unwrap();
    CHECK_THROWS_AS((void)(a == b), bad_access);
}

TEST_CASE("flatten collapses nested Result")
{
    auto inner = res::Result<int, std::string>::Ok(42);
    auto outer = res::Result<
        res::Result<int, std::string>,
        std::string>::Ok(std::move(inner));
    auto flat = outer.flatten();
    CHECK(flat.is_ok());
    CHECK(flat.unwrap() == 42);
}

TEST_CASE("flatten on Err propagates")
{
    auto outer = res::Result<
        res::Result<int, std::string>,
        std::string>::Err(std::string("e"));
    auto flat = outer.flatten();
    CHECK(flat.is_err());
    CHECK(flat.unwrap_err() == "e");
}

TEST_CASE("flatten rvalue moves inner Result")
{
    auto inner = res::Result<
        std::unique_ptr<int>,
        std::string>::Ok(std::unique_ptr<int>(new int(7)));
    auto outer = res::Result<
        res::Result<
            std::unique_ptr<int>,
            std::string>,
        std::string>::Ok(std::move(inner));
    auto flat = std::move(outer).flatten();
    CHECK(flat.is_ok());
    CHECK(*flat.unwrap() == 7);
}

TEST_CASE("flatten rvalue on Err propagates")
{
    auto outer = res::Result<
        res::Result<
            int,
            std::string>,
        std::string>::Err(std::string("e"));
    auto flat = std::move(outer).flatten();
    CHECK(flat.is_err());
    CHECK(flat.unwrap_err() == "e");
}

TEST_CASE("inspect rvalue observes only the active branch and returns by value")
{
    int ok_calls = 0;
    int err_calls = 0;

    auto ok = StrResult::Ok(7);
    auto moved_ok = std::move(ok).inspect(
        [&](int v)
        {
            ++ok_calls;
            CHECK(v == 7);
        });
    CHECK(ok_calls == 1);
    CHECK(moved_ok.is_ok());
    CHECK(moved_ok.unwrap() == 7);
    CHECK(ok.is_ok()); // source not marked moved

    auto er = StrResult::Err(std::string("e"));
    auto moved_err = std::move(er).inspect(
        [&](int v)
        {
            ++err_calls;
            (void)v;
        });
    CHECK(err_calls == 0); // not invoked on Err
    CHECK(ok_calls == 1);
    CHECK(moved_err.is_err());
    CHECK(er.is_err()); // source not marked moved
}

TEST_CASE("inspect_err rvalue observes only the error branch and returns by value")
{
    int calls = 0;

    auto er = StrResult::Err(std::string("boom"));
    auto moved_err = std::move(er).inspect_err(
        [&](const std::string &e)
        {
            ++calls;
            CHECK(e == "boom");
        });
    CHECK(calls == 1);
    CHECK(moved_err.is_err());
    CHECK(moved_err.unwrap_err() == "boom");
    CHECK(er.is_err()); // source not marked moved

    auto ok = StrResult::Ok(3);
    auto moved_ok = std::move(ok).inspect_err(
        [&](const std::string &e)
        {
            ++calls;
            (void)e;
        });
    CHECK(calls == 1); // not invoked on Ok
    CHECK(moved_ok.is_ok());
    CHECK(ok.is_ok());
}

TEST_CASE("inspect rvalue chains from a temporary Result")
{
    int seen = 0;
    auto r = make_ok(4)
                 .inspect(
                     [&](int v)
                     {
                         ++seen;
                         CHECK(v == 4);
                     })
                 .map(
                     [](int v)
                     { return v * 10; });
    CHECK(seen == 1);
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 40);
}

TEST_CASE("lvalue inspect keeps returning a reference to self")
{
    auto r = StrResult::Ok(1);
    const auto &same = r.inspect(&observe_ok);
    CHECK(&same == &r);
    CHECK(r.is_ok());
}

// 编译期：and_with 取 other 的成功类型 U 并保留 E；or_with 保留 T 并取 other 的错误类型 F
static_assert(std::is_same_v<
              decltype(std::declval<StrResult &>().and_with(
                  std::declval<res::Result<long, std::string>>())),
              res::Result<long, std::string>>);
static_assert(std::is_same_v<
              decltype(std::declval<StrResult &>().or_with(
                  std::declval<res::Result<int, std::size_t>>())),
              res::Result<int, std::size_t>>);
static_assert(std::is_same_v<
              decltype(std::declval<res::Result<void, std::string>>().and_with(
                  std::declval<res::Result<int, std::string>>())),
              res::Result<int, std::string>>);
static_assert(std::is_same_v<
              decltype(std::declval<res::Result<void, std::string>>().or_with(
                  std::declval<res::Result<void, std::size_t>>())),
              res::Result<void, std::size_t>>);

TEST_CASE("and_with returns other on Ok")
{
    auto r = res::Result<int, long>::Ok(3).and_with(
        res::Result<std::string, long>::Ok(std::string("x")));
    CHECK(r.is_ok());
    CHECK(r.unwrap() == "x");
}

TEST_CASE("and_with propagates the error on Err")
{
    auto r = StrResult::Err(std::string("e")).and_with(
        res::Result<long, std::string>::Ok(1L));
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == "e");
}

TEST_CASE("and_with const lvalue copies other")
{
    res::Result<int, long> base = res::Result<int, long>::Ok(1);
    res::Result<std::string, long> other =
        res::Result<std::string, long>::Ok(std::string("x"));
    auto r = base.and_with(other);
    CHECK(r.unwrap() == "x");
    CHECK(other.is_ok()); // other left intact
}

TEST_CASE("and_with rvalue moves a move-only other")
{
    auto r = StrResult::Ok(1).and_with(
        res::Result<std::unique_ptr<int>, std::string>::Ok(
            std::unique_ptr<int>(new int(9))));
    CHECK(r.is_ok());
    CHECK(*r.unwrap() == 9);
}

TEST_CASE("or_with keeps Ok and replaces Err")
{
    auto kept = StrResult::Ok(3).or_with(res::Result<int, std::size_t>::Ok(9));
    CHECK(kept.is_ok());
    CHECK(kept.unwrap() == 3);

    auto recovered =
        StrResult::Err(std::string("e")).or_with(res::Result<int, std::size_t>::Ok(9));
    CHECK(recovered.is_ok());
    CHECK(recovered.unwrap() == 9);

    auto still_err = StrResult::Err(std::string("e"))
                         .or_with(res::Result<int, std::size_t>::Err(std::size_t{7}));
    CHECK(still_err.is_err());
    CHECK(still_err.unwrap_err() == std::size_t{7});
}

TEST_CASE("or_with const lvalue copies the Ok value")
{
    StrResult base = StrResult::Ok(3);
    auto r = base.or_with(res::Result<int, std::size_t>::Ok(9));
    CHECK(r.unwrap() == 3);
    CHECK(base.is_ok());
}

TEST_CASE("or_with rvalue moves a move-only Ok value")
{
    auto base = res::Result<std::unique_ptr<int>, std::string>::Ok(
        std::unique_ptr<int>(new int(3)));
    auto r = std::move(base).or_with(
        res::Result<std::unique_ptr<int>, std::size_t>::Err(std::size_t{1}));
    CHECK(r.is_ok());
    CHECK(*r.unwrap() == 3);
}

TEST_CASE("Result<void, E>::and_with yields other or propagates")
{
    auto r = res::Result<void, std::string>::Ok().and_with(
        res::Result<int, std::string>::Ok(7));
    CHECK(r.unwrap() == 7);

    auto e = res::Result<void, std::string>::Err(std::string("e")).and_with(
        res::Result<int, std::string>::Ok(7));
    CHECK(e.is_err());
    CHECK(e.unwrap_err() == "e");
}

TEST_CASE("Result<void, E>::or_with keeps Ok or takes other")
{
    auto ok = res::Result<void, std::string>::Ok().or_with(
        res::Result<void, std::size_t>::Err(std::size_t{1}));
    CHECK(ok.is_ok());

    auto rec = res::Result<void, std::string>::Err(std::string("e")).or_with(
        res::Result<void, std::size_t>::Err(std::size_t{1}));
    CHECK(rec.is_err());
    CHECK(rec.unwrap_err() == std::size_t{1});
}

TEST_CASE("and_with and or_with throw on moved Result")
{
    auto r = StrResult::Ok(5);
    std::move(r).unwrap();
    CHECK_THROWS_AS(
        (void)r.and_with(res::Result<long, std::string>::Ok(1L)), bad_access);
    CHECK_THROWS_AS((void)r.or_with(res::Result<int, std::size_t>::Ok(9)), bad_access);
}
