// Expected to fail compilation: Option's static_assert should reject a type whose
// move constructor may throw. Driven by CMake as a WILL_FAIL test.
#include "pjh_result/option.hpp"
#include "support/instrumented.hpp"

int main()
{
    auto o = pjh::result::Option<pjh_test::ThrowingMove>::None();
    (void)o;
    return 0;
}
