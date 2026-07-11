// void_result.cpp — Result<void, E> for fallible operations without a success value.
#include <iostream>
#include <string>

#include "pjh_result/result.hpp"

namespace rp = pjh::result::utils;

using VoidResult = rp::Result<void, std::string>;
using IntResult = rp::Result<int, std::string>;

static VoidResult check_even(int x)
{
    if (x % 2 != 0)
        return VoidResult::Err(std::string("odd"));
    return VoidResult::Ok();
}

int main()
{
    VoidResult ok = check_even(4);
    std::cout << "check_even(4).is_ok() = " << std::boolalpha << ok.is_ok() << '\n';
    ok.unwrap(); // returns void; does not throw when Ok

    VoidResult bad = check_even(3);
    std::cout << "check_even(3).unwrap_err() = " << bad.unwrap_err() << '\n';

    // map a void result into a valued one, and chain into another Result.
    IntResult mapped = check_even(8)
                           .map([]() { return 100; })
                           .and_then([](int x) { return IntResult::Ok(x + 1); });
    std::cout << "mapped -> " << (mapped.is_ok() ? std::to_string(mapped.unwrap()) : mapped.unwrap_err()) << '\n';

    return 0;
}
