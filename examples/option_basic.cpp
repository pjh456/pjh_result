// option_basic.cpp — constructing, inspecting, and unwrapping an Option.
#include <iostream>
#include <string>

#include "pjh_result.hpp"

namespace res = pjh::result;

using StrOpt = res::Option<std::string>;

int main()
{
    StrOpt some = StrOpt::Some(std::string("hello"));
    StrOpt none = StrOpt::None();

    std::cout << "some.is_some() = " << std::boolalpha << some.is_some() << '\n';
    std::cout << "none.is_none() = " << none.is_none() << '\n';

    std::cout << "some.unwrap()            = " << some.unwrap() << '\n';
    std::cout << "none.unwrap_or(default)  = " << none.unwrap_or(std::string("default")) << '\n';

    return 0;
}
