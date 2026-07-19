#include <doctest/doctest.h>


#include "pjh_result/option.hpp"

namespace res = pjh::result;

using IntOpt = res::Option<int>;
using VoidOpt = res::Option<void>;

TEST_CASE("equal Some values compare equal")
{
    CHECK(IntOpt::Some(1) == IntOpt::Some(1));
    CHECK(IntOpt::Some(1) != IntOpt::Some(2));
}

TEST_CASE("None compares equal to None")
{
    CHECK(IntOpt::None() == IntOpt::None());
}

TEST_CASE("Some and None never compare equal")
{
    CHECK(IntOpt::Some(1) != IntOpt::None());
}

TEST_CASE("void options compare by presence")
{
    CHECK(VoidOpt::Some() == VoidOpt::Some());
    CHECK(VoidOpt::None() == VoidOpt::None());
    CHECK(VoidOpt::Some() != VoidOpt::None());
}
