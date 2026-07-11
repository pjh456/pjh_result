// 该文件预期编译失败：Result 的 static_assert 应拒绝“移动会抛”的类型。
// 由 CMake 以 WILL_FAIL 测试驱动（编译失败即视为通过），故 EXCLUDE_FROM_ALL。
#include "pjh_result/result.hpp"
#include "support/instrumented.hpp"

int main()
{
    auto r = pjh::result::utils::Result<pjh_test::ThrowingMove, int>::Err(1);
    (void)r;
    return 0;
}
