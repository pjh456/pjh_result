#include <cassert>

#include "pjh/option.hpp"
#include "pjh/result.hpp"

int main() {
    pjh::Result<int, int> r;
    pjh::Option<int> o;
    (void)r;
    (void)o;
    assert(true);
    return 0;
}
