#include <cassert>
#include <string>
#include <vector>

#include "pjh_result/macros.hpp"
#include "pjh_result/result.hpp"

using pjh::result::utils::Result;

// bug3: 移动值构造须条件化 noexcept —— throwing-move 类型下不得声称 noexcept
struct ThrowMove
{
    ThrowMove() = default;
    ThrowMove(ThrowMove &&) {}
};
static_assert(std::is_nothrow_constructible_v<Result<int, std::string>, int &&>);
static_assert(!std::is_nothrow_constructible_v<Result<ThrowMove, int>, ThrowMove &&>);

int main()
{
    auto ok = Result<int, std::string>::Ok(42);
    assert(ok.is_ok());
    assert(!ok.is_err());
    assert(ok.unwrap() == 42);

    auto err = Result<int, std::string>::Err(std::string("boom"));
    assert(err.is_err());
    assert(err.unwrap_err() == "boom");

    auto mapped = Result<int, std::string>::Ok(10).map([](int v)
                                                       { return v * 2; });
    assert(mapped.unwrap() == 20);

    // bug1: unwrap_or 曾返回临时参数的悬垂引用；按值收参后临时值须存活
    auto e = Result<std::string, int>::Err(5);
    assert(e.unwrap_or(std::string("fallback")) == "fallback");
    auto o2 = Result<std::string, int>::Ok(std::string("hi"));
    assert(o2.unwrap_or(std::string("x")) == "hi");

    // bug2: explicit 拷贝/移动构造曾挡住左值拷贝初始化与容器存储
    auto src = Result<int, std::string>::Ok(7);
    auto copy = src;
    assert(copy.unwrap() == 7);
    std::vector<Result<int, std::string>> v;
    v.push_back(Result<int, std::string>::Ok(1));
    v.push_back(src);
    assert(v.size() == 2 && v[0].unwrap() == 1 && v[1].unwrap() == 7);

    return 0;
}
