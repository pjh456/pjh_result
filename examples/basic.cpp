// basic.cpp — constructing, inspecting, and unwrapping a Result.
#include <iostream>
#include <string>

#include "pjh_result/result.hpp"

namespace res = pjh::result;

using IntResult = res::Result<int, std::string>;

int main()
{
    IntResult ok = IntResult::Ok(42);
    IntResult err = IntResult::Err(std::string("something went wrong"));

    std::cout << "ok.is_ok()  = " << std::boolalpha << ok.is_ok() << '\n';
    std::cout << "err.is_err() = " << err.is_err() << '\n';

    std::cout << "ok.unwrap()          = " << ok.unwrap() << '\n';
    std::cout << "err.unwrap_or(-1)    = " << err.unwrap_or(-1) << '\n';
    std::cout << "err.unwrap_err()     = " << err.unwrap_err() << '\n';

    return 0;
}
