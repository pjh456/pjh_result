// error_prop.cpp — Rust `?`-style propagation via ASSIGN_OR_RETURN and TRY.
#include <iostream>
#include <string>

#include "pjh_result/macros.hpp"
#include "pjh_result/result.hpp"

namespace res = pjh::result;

using IntResult = res::Result<int, std::string>;
using VoidResult = res::Result<void, std::string>;

static IntResult parse_positive(int x)
{
    if (x <= 0)
        return IntResult::Err(std::string("not positive"));
    return IntResult::Ok(x);
}

// Propagates the first Err encountered, otherwise returns the sum.
static IntResult sum_positive(int a, int b)
{
    ASSIGN_OR_RETURN(x, parse_positive(a));
    ASSIGN_OR_RETURN(y, parse_positive(b));
    return IntResult::Ok(x + y);
}

// TRY discards the Ok value but still short-circuits on Err.
static VoidResult validate(int a, int b)
{
    TRY(parse_positive(a));
    TRY(parse_positive(b));
    return VoidResult::Ok();
}

int main()
{
    auto good = sum_positive(3, 4);
    std::cout << "sum_positive(3, 4) -> " << (good.is_ok() ? std::to_string(good.unwrap()) : good.unwrap_err()) << '\n';

    auto bad = sum_positive(3, -1);
    std::cout << "sum_positive(3,-1) -> " << (bad.is_ok() ? std::to_string(bad.unwrap()) : bad.unwrap_err()) << '\n';

    auto v = validate(1, -2);
    std::cout << "validate(1,-2) is_err = " << std::boolalpha << v.is_err() << " (" << v.unwrap_err() << ")\n";

    return 0;
}
