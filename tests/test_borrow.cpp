#include <doctest/doctest.h>

#include <functional>
#include <string>
#include <type_traits>
#include <utility>

#include "pjh_result.hpp"

namespace res = pjh::result;

using IntOpt = res::Option<int>;
using StrResult = res::Result<int, std::string>;
using VoidErrorResult = res::Result<void, std::string>;

// 编译期：借用视图的精确返回类型
static_assert(std::is_same_v<
              decltype(std::declval<const IntOpt &>().as_ref()),
              res::Option<std::reference_wrapper<const int>>>);
static_assert(std::is_same_v<
              decltype(std::declval<IntOpt &>().as_mut()),
              res::Option<std::reference_wrapper<int>>>);
static_assert(std::is_same_v<
              decltype(std::declval<const StrResult &>().as_ref()),
              res::Result<std::reference_wrapper<const int>,
                          std::reference_wrapper<const std::string>>>);
static_assert(std::is_same_v<
              decltype(std::declval<StrResult &>().as_mut()),
              res::Result<std::reference_wrapper<int>,
                          std::reference_wrapper<std::string>>>);
static_assert(std::is_same_v<
              decltype(std::declval<const VoidErrorResult &>().as_ref()),
              res::Result<void, std::reference_wrapper<const std::string>>>);
static_assert(std::is_same_v<
              decltype(std::declval<VoidErrorResult &>().as_mut()),
              res::Result<void, std::reference_wrapper<std::string>>>);

// requires 表达式必须置于模板（concept）上下文中，否则非法调用是硬错误
template <typename O>
concept ConstAsRef = requires(const O &o) { o.as_ref(); };
template <typename O>
concept ConstAsMut = requires(const O &o) { o.as_mut(); };
template <typename O>
concept LvalueAsMut = requires(O &o) { o.as_mut(); };
template <typename O>
concept RvalueAsMut = requires(O o) { std::move(o).as_mut(); };
template <typename O>
concept RvalueAsRef = requires(O &&o) { std::move(o).as_ref(); };
template <typename O>
concept ConstRvalueAsRef = requires(const O &&o) { std::move(o).as_ref(); };

// 编译期：cv 限定与可用性
static_assert(ConstAsRef<IntOpt>);
static_assert(!ConstAsMut<IntOpt>);
static_assert(ConstAsRef<StrResult>);
static_assert(!ConstAsMut<StrResult>);
static_assert(LvalueAsMut<IntOpt>);
static_assert(LvalueAsMut<StrResult>);

// 编译期：T = void 时 Option 无值可借，禁用两种视图
static_assert(!ConstAsRef<res::Option<void>>);
static_assert(!ConstAsMut<res::Option<void>>);
static_assert(!LvalueAsMut<res::Option<void>>);

// 编译期：非 const 左值才可取可变借用，右值不可
static_assert(!RvalueAsMut<IntOpt>);
static_assert(!RvalueAsMut<StrResult>);

// 编译期：右值（含 const 右值）不可取借用视图，避免悬垂
static_assert(!RvalueAsRef<IntOpt>);
static_assert(!RvalueAsRef<StrResult>);
static_assert(!RvalueAsRef<VoidErrorResult>);
static_assert(!RvalueAsRef<res::Option<void>>);
static_assert(!ConstRvalueAsRef<IntOpt>);
static_assert(!ConstRvalueAsRef<StrResult>);
static_assert(!ConstRvalueAsRef<VoidErrorResult>);
static_assert(!ConstRvalueAsRef<res::Option<void>>);

TEST_CASE("Option::as_ref borrows Some and keeps None")
{
    auto o = IntOpt::Some(5);
    auto v = o.as_ref();
    CHECK(v.is_some());
    CHECK(v.unwrap().get() == 5);
    CHECK(&v.unwrap().get() == &o.unwrap());

    auto n = IntOpt::None();
    CHECK(n.as_ref().is_none());
}

TEST_CASE("Option::as_mut mutation is visible on the source")
{
    auto o = IntOpt::Some(5);
    auto v = o.as_mut();
    CHECK(v.is_some());
    v.unwrap().get() = 42;
    CHECK(o.unwrap() == 42);

    auto n = IntOpt::None();
    CHECK(n.as_mut().is_none());
}

TEST_CASE("Result::as_ref borrows the active branch")
{
    auto ok = StrResult::Ok(7);
    auto v = ok.as_ref();
    CHECK(v.is_ok());
    CHECK(v.unwrap().get() == 7);
    CHECK(&v.unwrap().get() == &ok.unwrap());

    auto er = StrResult::Err(std::string("boom"));
    auto w = er.as_ref();
    CHECK(w.is_err());
    CHECK(w.unwrap_err().get() == "boom");
    CHECK(&w.unwrap_err().get() == &er.unwrap_err());
}

TEST_CASE("Result::as_mut mutation is visible on the source")
{
    auto ok = StrResult::Ok(7);
    ok.as_mut().unwrap().get() = 8;
    CHECK(ok.unwrap() == 8);

    auto er = StrResult::Err(std::string("boom"));
    er.as_mut().unwrap_err().get() = "bang";
    CHECK(er.unwrap_err() == "bang");
}

TEST_CASE("Result<void, E> borrows only the error")
{
    auto ok = VoidErrorResult::Ok();
    CHECK(ok.as_ref().is_ok());
    CHECK(ok.as_mut().is_ok());

    auto er = VoidErrorResult::Err(std::string("bad"));
    auto w = er.as_ref();
    CHECK(w.is_err());
    CHECK(w.unwrap_err().get() == "bad");
    CHECK(&w.unwrap_err().get() == &er.unwrap_err());

    auto m = er.as_mut();
    CHECK(m.is_err());
    m.unwrap_err().get() = "worse";
    CHECK(er.unwrap_err() == "worse");
}

TEST_CASE("Result::as_ref and as_mut throw when moved")
{
    auto r = StrResult::Ok(1);
    (void)std::move(r).unwrap();
    CHECK(r.is_moved());
    CHECK_THROWS_AS((void)r.as_ref(), res::bad_result_access);
    CHECK_THROWS_AS((void)r.as_mut(), res::bad_result_access);
}

TEST_CASE("as_ref views compose with map and and_then")
{
    auto o = IntOpt::Some(20);
    auto mapped = o.as_ref().map(
        [](const int &x)
        { return x + 1; });
    CHECK(mapped.unwrap() == 21);

    auto chained = o.as_ref().and_then(
        [](const int &x)
        { return IntOpt::Some(x * 2); });
    CHECK(chained.unwrap() == 40);

    auto r = StrResult::Ok(3);
    auto rmapped = r.as_ref().map(
        [](const int &x)
        { return x * 3; });
    CHECK(rmapped.unwrap() == 9);
}
