// Expected to fail compilation: a type whose kind() returns void does not satisfy the
// Diagnostic concept's equality_comparable requirement. Driven by CMake as a WILL_FAIL
// test (compilation failure == test pass), hence EXCLUDE_FROM_ALL.
#include <string_view>

#include "pjh_result/diagnostic.hpp"

struct BadKind
{
    std::string_view message() const { return "m"; }
    void kind() const {}
};

template <pjh::result::Diagnostic E>
void require_diagnostic(const E &) {}

int main()
{
    require_diagnostic(BadKind{});
    return 0;
}
