# pjh_result

A header-only, C++20 port of Rust's `Result<T, E>` and `Option<T>` for expressive,
exception-light error handling.

`Result<T, E>` is in exactly one of three states: **Ok** (value `T`), **Err** (error `E`), or
**Moved** (after an rvalue unwrap/extract). `Option<T>` holds **either** a value (`Some`)
**or** nothing (`None`). Both use hand-written tagged-union storage — no `std::variant`,
no `valueless_by_exception`.

## Features

- **Three-state `Result`** — Ok / Err / Moved (moved-from), detectable via `is_moved()`.
- **`T = void` support** — for fallible operations or optional signals that carry no value.
- **Monadic combinators** — `map`, `map_err`, `and_then`, `or_else`, `flatten`, `filter`, `zip`, `zip_with`, `x_or`, `transpose`.
- **Rich query API** — `is_ok_and`, `is_err_and`, `is_none_and`, `contains`, `contains_err`.
- **Rust `?`-style propagation** — the `ASSIGN_OR_RETURN` and `TRY` macros.
- **Strong exception guarantee** on assignment; `nothrow`-move invariant enforced at compile time.
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
    auto r = 
        parse_positive(4)
        .map([](int x) { return x * 2; })
        .and_then([](int x) { return sum_positive(x, 1); });

    if (r.is_ok())
        return r.unwrap();
    return -1;
}
```

## Result API at a glance

| Category | Members |
|---|---|
| Construct | `Ok(v)`, `Ok()` (when `T = void`), `Err(e)` |
| Inspect & query | `is_ok()`, `is_err()`, `is_moved()`, `is_ok_and(f)`, `is_err_and(f)`, `contains(v)`, `contains_err(e)`, `operator==` |
| Extract | `unwrap()`, `unwrap_or(v)`, `unwrap_or_else(f)`, `unwrap_or_default()`, `expect(msg)`, `unwrap_err()`, `unwrap_err_or(e)`, `expect_err(msg)` |
| Transform | `map(f)`, `map_err(f)`, `map_or(def, f)`, `map_or_else(d, f)`, `inspect(f)`, `inspect_err(f)` |
| Chain & flatten | `and_then(f)`, `or_else(f)`, `flatten()` |
| Transpose | `transpose()` → `Option<Result<U,E>>` ↔ `Result<Option<U>,E>` |
| Propagate | `ASSIGN_OR_RETURN(name, expr)`, `TRY(expr)` (from `macros.hpp`) |

Accessor methods on the wrong state (`unwrap()` on `Err` or `Moved`, etc.) throw
`pjh::result::bad_result_access`. Combinators (`map`, `and_then`, etc.) throw
`pjh::result::bad_result_access` when called on a `Result` in the `Moved` state.

## Option API at a glance

`pjh::result::Option<T>` mirrors the same design (hand-written union storage,
`nothrow`-move invariant, strong exception guarantee, `T = void` support).

| Category | Members |
|---|---|
| Construct | `Some(v)`, `Some()` (when `T = void`), `None()` |
| Inspect & query | `is_some()`, `is_none()`, `is_some_and(f)`, `is_none_and(f)`, `contains(v)`, `operator==` |
| Extract | `unwrap()`, `unwrap_or(v)`, `unwrap_or_else(f)`, `unwrap_or_default()`, `expect(msg)` |
| Transform | `map(f)`, `map_or(def, f)`, `map_or_else(d, f)`, `inspect(f)` |
| Chain, flatten & zip | `and_then(f)`, `or_else(f)`, `filter(pred)`, `flatten()`, `zip(Option)`, `zip_with(Option, f)` |
| Exclusive or | `x_or(Option)` |
| Mutate | `take()`, `replace(v)`, `insert(v)`, `get_or_insert(v)`, `get_or_insert_default()`, `get_or_insert_with(f)` |
| To Result | `ok_or(e)`, `ok_or_else(f)` |
| Transpose | `transpose()` → `Result<Option<U>,E>` ↔ `Option<Result<U,E>>` |

`unwrap`/`expect` throw `pjh::result::bad_result_access` when called on `None`.

## Converting between the two

`Option → Result` is a member (`ok_or` / `ok_or_else`). The reverse direction lives in
`pjh_result/interop.hpp` as free functions (kept out of the class headers to avoid a
circular include):

```cpp
#include "pjh_result/interop.hpp"

res::Result<int, std::string> r = /* ... */;
res::Option<int> maybe = res::ok(r);   // Ok -> Some, Err -> None (discards error)
res::Option<std::string> e = res::err(r); // Err -> Some, Ok -> None
```

## Examples

Runnable programs under [`examples/`](examples/):

| File | Shows |
|---|---|
| `basic.cpp` | construction, inspection, unwrapping |
| `pipeline.cpp` | chaining `map` / `and_then` / `map_err` |
| `error_prop.cpp` | `ASSIGN_OR_RETURN` and `TRY` propagation |
| `void_result.cpp` | `Result<void, E>` |
| `custom_error.cpp` | enum and struct error types |
| `option_basic.cpp` | `Option` construction, inspection, unwrapping |
| `option_chain.cpp` | chaining `map` / `filter` / `and_then` / `or_else` |
| `option_interop.cpp` | `Option` → `Result` via `ok_or` / `ok_or_else` |

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
