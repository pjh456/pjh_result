// option_chain.cpp — composing with map / filter / and_then / or_else.
#include <iostream>
#include <string>

#include "pjh_result.hpp"

namespace res = pjh::result;

using IntOpt = res::Option<int>;

int main()
{
    auto r = IntOpt::Some(4)
                 .map([](int x) { return x * 3; })          // Some(12)
                 .filter([](int x) { return x % 2 == 0; })   // stays Some(12)
                 .and_then([](int x) { return IntOpt::Some(x + 1); }); // Some(13)

    std::cout << "chain    -> " << (r.is_some() ? std::to_string(r.unwrap()) : "none") << '\n';

    auto recovered = IntOpt::None().or_else([]() { return IntOpt::Some(-1); });
    std::cout << "or_else  -> " << recovered.unwrap() << '\n';

    auto dropped = IntOpt::Some(3).filter([](int x) { return x % 2 == 0; });
    std::cout << "filtered -> " << (dropped.is_some() ? std::to_string(dropped.unwrap()) : "none") << '\n';

    return 0;
}
