#include <doctest/doctest.h>

#include <algorithm>
#include <concepts>
#include <iterator>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>

#include "pjh_result.hpp"
#include "support/instrumented.hpp"

namespace res = pjh::result;

using IntOpt = res::Option<int>;
using StrResult = res::Result<int, std::string>;
using VoidOpt = res::Option<void>;
using VoidResult = res::Result<void, std::string>;
using bad_access = res::bad_result_access;

// ---------------------------------------------------------------------------
// Compile-time: availability by value category and cv-qualification.
// A requires expression sees a deleted overload as unavailable.
// ---------------------------------------------------------------------------
template <typename O>
concept HasLvalueIter = requires(O &o) { o.iter(); };
template <typename O>
concept HasRvalueIter = requires(O o) { std::move(o).iter(); };
template <typename O>
concept HasConstRvalueIter = requires(const O o) { std::move(o).iter(); };
template <typename O>
concept HasLvalueIterMut = requires(O &o) { o.iter_mut(); };
template <typename O>
concept HasRvalueIterMut = requires(O o) { std::move(o).iter_mut(); };
template <typename O>
concept HasConstIterMut = requires(const O &o) { o.iter_mut(); };
template <typename O>
concept HasLvalueBegin = requires(O &o) { o.begin(); };
template <typename O>
concept HasConstBegin = requires(const O &o) { o.begin(); };
template <typename O>
concept HasLvalueEnd = requires(O &o) { o.end(); };
template <typename O>
concept HasConstEnd = requires(const O &o) { o.end(); };

// Option: value categories.
static_assert(HasLvalueIter<IntOpt>);
static_assert(!HasRvalueIter<IntOpt>);
static_assert(!HasConstRvalueIter<IntOpt>);
static_assert(HasLvalueIterMut<IntOpt>);
static_assert(!HasRvalueIterMut<IntOpt>);
static_assert(!HasConstIterMut<IntOpt>);
static_assert(HasLvalueBegin<IntOpt>);
static_assert(HasConstBegin<IntOpt>);
static_assert(HasLvalueEnd<IntOpt>);
static_assert(HasConstEnd<IntOpt>);

// Result: value categories.
static_assert(HasLvalueIter<StrResult>);
static_assert(!HasRvalueIter<StrResult>);
static_assert(!HasConstRvalueIter<StrResult>);
static_assert(HasLvalueIterMut<StrResult>);
static_assert(!HasRvalueIterMut<StrResult>);
static_assert(!HasConstIterMut<StrResult>);
static_assert(HasLvalueBegin<StrResult>);
static_assert(HasConstBegin<StrResult>);
static_assert(HasLvalueEnd<StrResult>);
static_assert(HasConstEnd<StrResult>);

// T = void disables every iteration entry (no Unit& leaks out).
static_assert(!HasLvalueIter<VoidOpt>);
static_assert(!HasLvalueIterMut<VoidOpt>);
static_assert(!HasLvalueBegin<VoidOpt>);
static_assert(!HasConstBegin<VoidOpt>);
static_assert(!HasLvalueEnd<VoidOpt>);
static_assert(!HasConstEnd<VoidOpt>);
static_assert(!HasLvalueIter<VoidResult>);
static_assert(!HasLvalueIterMut<VoidResult>);
static_assert(!HasLvalueBegin<VoidResult>);
static_assert(!HasConstBegin<VoidResult>);
static_assert(!HasLvalueEnd<VoidResult>);
static_assert(!HasConstEnd<VoidResult>);

// ---------------------------------------------------------------------------
// Compile-time: exact reference types and iterator concept compliance.
// ---------------------------------------------------------------------------
static_assert(std::is_same_v<decltype(*std::declval<const IntOpt &>().iter()), const int &>);
static_assert(std::is_same_v<decltype(*std::declval<IntOpt &>().iter()), const int &>);
static_assert(std::is_same_v<decltype(*std::declval<IntOpt &>().iter_mut()), int &>);
static_assert(std::is_same_v<decltype(*std::declval<const StrResult &>().iter()), const int &>);
static_assert(std::is_same_v<decltype(*std::declval<StrResult &>().iter_mut()), int &>);

static_assert(std::forward_iterator<IntOpt::Iter>);
static_assert(std::forward_iterator<IntOpt::IterMut>);
static_assert(std::forward_iterator<StrResult::Iter>);
static_assert(std::forward_iterator<StrResult::IterMut>);

static_assert(std::ranges::input_range<IntOpt::Iter>);
static_assert(std::ranges::input_range<IntOpt::IterMut>);
static_assert(std::ranges::input_range<StrResult::Iter>);
static_assert(std::ranges::input_range<decltype(std::declval<const StrResult &>().iter())>);

// ---------------------------------------------------------------------------
// Option: iteration semantics.
// ---------------------------------------------------------------------------
TEST_CASE("Option::iter yields one element for Some and none for None")
{
    auto o = IntOpt::Some(42);
    CHECK(std::ranges::distance(o.iter()) == 1);
    CHECK(*o.iter() == 42);
    CHECK(&*o.iter() == &o.unwrap());

    auto n = IntOpt::None();
    CHECK(n.iter().begin() == n.iter().end());
    CHECK(std::ranges::distance(n.iter()) == 0);
    CHECK(std::distance(n.begin(), n.end()) == 0);
}

TEST_CASE("Option::iter_mut mutates the source")
{
    auto o = IntOpt::Some(42);
    *o.iter_mut() = 7;
    CHECK(o.unwrap() == 7);

    for (int &x : o.iter_mut())
        x += 1;
    CHECK(o.unwrap() == 8);

    auto n = IntOpt::None();
    CHECK(n.iter_mut().begin() == n.iter_mut().end());
}

TEST_CASE("Option is directly range-for-able")
{
    auto o = IntOpt::Some(1);
    for (auto &x : o)
        x = 3;
    CHECK(o.unwrap() == 3);

    int count = 0;
    for (const auto &x : std::as_const(o))
    {
        static_assert(std::is_same_v<decltype(x), const int &>);
        CHECK(x == 3);
        ++count;
    }
    CHECK(count == 1);

    auto n = IntOpt::None();
    count = 0;
    for (int x : n)
    {
        (void)x;
        ++count;
    }
    CHECK(count == 0);
}

TEST_CASE("Option iterators compose with classic and ranges algorithms")
{
    auto o = IntOpt::Some(5);
    CHECK(std::find(o.begin(), o.end(), 5) != o.end());
    CHECK(std::find(o.begin(), o.end(), 6) == o.end());

    auto it = o.iter();
    CHECK(std::ranges::find(it, 5) != it.end());
    CHECK(std::ranges::find(it, 6) == it.end());

    const auto c = IntOpt::Some(5);
    CHECK(std::find(c.begin(), c.end(), 5) != c.end());
    auto cit = c.iter();
    CHECK(std::ranges::find(cit, 6) == cit.end());
}

// ---------------------------------------------------------------------------
// Result: iteration semantics.
// ---------------------------------------------------------------------------
TEST_CASE("Result::iter yields one element for Ok and none for Err")
{
    auto ok = StrResult::Ok(5);
    CHECK(std::ranges::distance(ok.iter()) == 1);
    CHECK(*ok.iter() == 5);
    CHECK(&*ok.iter() == &ok.unwrap());

    auto er = StrResult::Err(std::string("e"));
    CHECK(er.is_err());
    CHECK(er.iter().begin() == er.iter().end());
    CHECK(std::ranges::distance(er.iter()) == 0);
    CHECK(std::distance(er.begin(), er.end()) == 0);
}

TEST_CASE("Result::iter_mut mutates the source")
{
    auto ok = StrResult::Ok(5);
    *ok.iter_mut() = 7;
    CHECK(ok.unwrap() == 7);

    for (int &x : ok.iter_mut())
        x += 1;
    CHECK(ok.unwrap() == 8);

    auto er = StrResult::Err(std::string("e"));
    CHECK(er.iter_mut().begin() == er.iter_mut().end());
    CHECK(er.unwrap_err() == "e");
}

TEST_CASE("Result is directly range-for-able")
{
    auto ok = StrResult::Ok(1);
    for (auto &x : ok)
        x = 3;
    CHECK(ok.unwrap() == 3);

    int count = 0;
    for (const auto &x : std::as_const(ok))
    {
        static_assert(std::is_same_v<decltype(x), const int &>);
        CHECK(x == 3);
        ++count;
    }
    CHECK(count == 1);

    auto er = StrResult::Err(std::string("e"));
    count = 0;
    for (int x : er)
    {
        (void)x;
        ++count;
    }
    CHECK(count == 0);
}

TEST_CASE("range-for over a temporary Result extends its lifetime")
{
    int sum = 0;
    for (int x : StrResult::Ok(2))
        sum += x;
    CHECK(sum == 2);

    int count = 0;
    for (int x : StrResult::Err(std::string("boom")))
    {
        (void)x;
        ++count;
    }
    CHECK(count == 0);
}

TEST_CASE("Result iterators compose with classic and ranges algorithms")
{
    auto ok = StrResult::Ok(5);
    CHECK(std::find(ok.begin(), ok.end(), 5) != ok.end());
    CHECK(std::find(ok.begin(), ok.end(), 6) == ok.end());

    auto it = ok.iter();
    CHECK(std::ranges::find(it, 5) != it.end());
    CHECK(std::ranges::find(it, 6) == it.end());

    const auto c = StrResult::Ok(5);
    CHECK(std::find(c.begin(), c.end(), 5) != c.end());
    auto cit = c.iter();
    CHECK(std::ranges::find(cit, 6) == cit.end());
}

TEST_CASE("Result iteration throws on a moved-from result")
{
    auto r = StrResult::Ok(1);
    (void)std::move(r).unwrap();
    CHECK(r.is_moved());

    CHECK_THROWS_AS((void)r.iter(), bad_access);
    CHECK_THROWS_AS((void)r.iter_mut(), bad_access);
    CHECK_THROWS_AS((void)r.begin(), bad_access);
    CHECK_THROWS_AS((void)std::as_const(r).iter(), bad_access);
    CHECK_THROWS_AS((void)std::as_const(r).begin(), bad_access);

    CHECK_NOTHROW((void)r.end());
    CHECK_NOTHROW((void)std::as_const(r).end());
}

// ---------------------------------------------------------------------------
// Iteration is a borrow: no element is copied.
// ---------------------------------------------------------------------------
TEST_CASE("Option iteration does not copy the element")
{
    pjh_test::InstanceCounter::reset();
    {
        auto o = res::Option<pjh_test::InstanceCounter>::Some(
            pjh_test::InstanceCounter{3});
        const int ctor_before = pjh_test::InstanceCounter::ctor;

        int sum = 0;
        for (const auto &x : o)
            sum += x.id;
        auto it = o.iter();
        CHECK(it->id == 3);
        sum += it->id;
        CHECK(sum == 6);

        CHECK(pjh_test::InstanceCounter::ctor == ctor_before);
        CHECK(pjh_test::InstanceCounter::live == 1);
    }
    CHECK(pjh_test::InstanceCounter::live == 0);
}

TEST_CASE("Result iteration does not copy the element")
{
    pjh_test::InstanceCounter::reset();
    {
        auto r = res::Result<pjh_test::InstanceCounter, std::string>::Ok(
            pjh_test::InstanceCounter{4});
        const int ctor_before = pjh_test::InstanceCounter::ctor;

        int sum = 0;
        for (const auto &x : r)
            sum += x.id;
        auto it = r.iter();
        CHECK(it->id == 4);
        sum += it->id;
        CHECK(sum == 8);

        CHECK(pjh_test::InstanceCounter::ctor == ctor_before);
        CHECK(pjh_test::InstanceCounter::live == 1);
    }
    CHECK(pjh_test::InstanceCounter::live == 0);
}
