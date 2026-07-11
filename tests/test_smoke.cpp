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

    // bug1: unwrap_or 曾返回临时参数的悬垂引用；按值收参后临时值须存活
    auto e = Result<std::string, int>::Err(5);
    assert(e.unwrap_or(std::string("fallback")) == "fallback");
    auto o2 = Result<std::string, int>::Ok(std::string("hi"));
    assert(o2.unwrap_or(std::string("x")) == "hi");

    return 0;
}
