# pjh_result

A header-only, C++20 port of Rust's `Result<T, E>` for expressive, exception-light error handling.

`Result<T, E>` holds **either** a success value `T` **or** an error `E` — never both, never
neither. It forces callers to acknowledge failure instead of silently ignoring it.

## Features

- **No `std::variant`** — hand-written tagged-union storage.
- **No invalid "third state"** — `is_ok()` and `is_err()` are always complementary
  (no `valueless_by_exception`).
- **Strong exception guarantee** on assignment; a `nothrow`-move invariant is enforced at
  compile time via `static_assert`.
- **`T = void` support** — for fallible operations that return no value.
- **Monadic combinators** — `map`, `map_err`, `and_then`.
- **Rust `?`-style propagation** — the `ASSIGN_OR_RETURN` and `TRY` macros.
- **Header-only**, no runtime dependencies.

## Requirements

- A C++20 compiler (concepts, `requires`-clauses).

## Integration

The library is an `INTERFACE` CMake target named `pjh_result`.

### As a subdirectory / submodule

```cmake
add_subdirectory(third_party/pjh_result)
target_link_libraries(your_target PRIVATE pjh_result)
```

When consumed this way, its tests and examples are **not** built.

### Manually

Add `include/` to your include path — there is nothing to compile.

## Quick start

```cpp
#include "pjh_result/result.hpp"
#include "pjh_result/macros.hpp"

namespace res = pjh::result;
using IntResult = res::Result<int, std::string>;

IntResult parse_positive(int x)
{
    if (x <= 0)
        return IntResult::Err(std::string("not positive"));
    return IntResult::Ok(x);
}

IntResult sum_positive(int a, int b)
{
    ASSIGN_OR_RETURN(x, parse_positive(a)); // returns early on Err
    ASSIGN_OR_RETURN(y, parse_positive(b));
    return IntResult::Ok(x + y);
}

int main()
{
    auto r = parse_positive(4)
                 .map([](int x) { return x * 2; })
                 .and_then([](int x) { return sum_positive(x, 1); });

    if (r.is_ok())
        return r.unwrap();
    return -1;
}
```

## API at a glance

| Category | Members |
|---|---|
| Construct | `Ok(v)`, `Ok()` (when `T = void`), `Err(e)` |
| Inspect | `is_ok()`, `is_err()`, `is_ok_and(f)`, `is_err_and(f)`, `operator==` |
| Extract | `unwrap()`, `unwrap_or(v)`, `unwrap_or_else(f)`, `unwrap_or_default()`, `expect(msg)`, `unwrap_err()`, `unwrap_err_or(e)`, `expect_err(msg)` |
| Transform | `map(f)`, `map_err(f)`, `map_or(def, f)`, `map_or_else(d, f)`, `inspect(f)`, `inspect_err(f)` |
| Chain | `and_then(f)`, `or_else(f)` |
| Propagate | `ASSIGN_OR_RETURN(name, expr)`, `TRY(expr)` (from `macros.hpp`) |

`unwrap*` and `expect*` throw `pjh::result::bad_result_access` on the wrong state.

> **Note:** `T` and `E` must be distinct types, and their move constructors must be
> `noexcept`. Both are enforced at compile time.

## Examples

Runnable programs under [`examples/`](examples/):

| File | Shows |
|---|---|
| `basic.cpp` | construction, inspection, unwrapping |
| `pipeline.cpp` | chaining `map` / `and_then` / `map_err` |
| `error_prop.cpp` | `ASSIGN_OR_RETURN` and `TRY` propagation |
| `void_result.cpp` | `Result<void, E>` |
| `custom_error.cpp` | enum and struct error types |

## Building tests & examples

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

Toggle with `-DPJH_RESULT_BUILD_TESTS=ON/OFF` and `-DPJH_RESULT_BUILD_EXAMPLES=ON/OFF`
(both default to ON when this is the top-level project). Tests use
[doctest](https://github.com/doctest/doctest), vendored as a git submodule under
`tests/third_party/` — run `git submodule update --init --depth 1` first.

## License

MIT — see [LICENSE](LICENSE).
