// option_interop.cpp — bridging Option into Result via ok_or / ok_or_else.
#include <iostream>
#include <string>

#include "pjh_result.hpp"

namespace res = pjh::result;

using IntOpt = res::Option<int>;
using IntResult = res::Result<int, std::string>;

static void report(const char *label, const IntResult &r)
{
    std::cout << label << " -> "
              << (r.is_ok() ? std::to_string(r.unwrap()) : r.unwrap_err()) << '\n';
}

int main()
{
    IntResult ok = IntOpt::Some(42).ok_or(std::string("missing"));
    report("Some.ok_or       ", ok);

    IntResult err = IntOpt::None().ok_or(std::string("missing"));
    report("None.ok_or       ", err);

    IntResult lazy = IntOpt::None().ok_or_else([]() { return std::string("computed"); });
    report("None.ok_or_else  ", lazy);

    return 0;
}
