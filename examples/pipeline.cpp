// pipeline.cpp — composing transformations with map / and_then / map_err.
#include <iostream>
#include <string>

#include "pjh_result.hpp"

namespace res = pjh::result;

using IntResult = res::Result<int, std::string>;

// Succeeds only for non-negative inputs.
static IntResult checked(int x)
{
    if (x < 0)
        return IntResult::Err(std::string("negative"));
    return IntResult::Ok(x);
}

int main()
{
    auto r = IntResult::Ok(5)
                 .map([](int x) { return x * 2; })          // 5 -> 10
                 .and_then([](int x) { return checked(x); }) // stays Ok(10)
                 .map_err([](const std::string &e) { return "error: " + e; });

    std::cout << "pipeline ok  -> " << (r.is_ok() ? std::to_string(r.unwrap()) : r.unwrap_err()) << '\n';

    auto bad = IntResult::Ok(-3)
                   .and_then([](int x) { return checked(x); }) // becomes Err("negative")
                   .map_err([](const std::string &e) { return "error: " + e; });

    std::cout << "pipeline bad -> " << (bad.is_ok() ? std::to_string(bad.unwrap()) : bad.unwrap_err()) << '\n';

    return 0;
}
