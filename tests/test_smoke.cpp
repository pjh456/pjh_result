#include <cassert>
#include <string>

#include "pjh_result/macros.hpp"
#include "pjh_result/result.hpp"

using pjh::result::utils::Result;

int main() {
    auto ok = Result<int, std::string>::Ok(42);
    assert(ok.is_ok());
    assert(!ok.is_err());
    assert(ok.unwrap() == 42);

    auto err = Result<int, std::string>::Err(std::string("boom"));
    assert(err.is_err());
    assert(err.unwrap_err() == "boom");

    auto mapped = Result<int, std::string>::Ok(10).map([](int v) { return v * 2; });
    assert(mapped.unwrap() == 20);

    return 0;
}
