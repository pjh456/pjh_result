# pjh_result

A header-only, C++20 port of Rust's `Result<T, E>` and `Option<T>` for expressive,
exception-light error handling — with an error-context chain, a `Diagnostic` protocol,
borrowing views, iteration, and optional `std::format` / `operator<<` interop.

`Result<T, E>` is in exactly one of three states: **Ok** (value `T`), **Err** (error `E`), or
**Moved** (after an rvalue unwrap/extract). `Option<T>` holds **either** a value (`Some`)
**or** nothing (`None`). Both use hand-written tagged-union storage — no `std::variant`,
no `valueless_by_exception`.

## Features

- **Three-state `Result`** — Ok / Err / Moved (moved-from), detectable via `is_moved()`.
- **`T = void` support** — for fallible operations or optional signals that carry no value.
- **Monadic combinators** — `map`, `map_err`, `map_or`, `map_or_else`, `and_then`, `or_else`,
  `and_with`, `or_with`, `flatten`, `filter`, `zip`, `zip_with`, `unzip`, `x_or`, `transpose`.
- **Rich query & extract API** — `is_ok_and`, `is_err_and`, `is_some_and`, `is_none_and`,
  `contains`, `contains_err`, `inspect`, `inspect_err`, `unwrap_err_or`,
  `unwrap_err_or_else`, and the usual `unwrap` / `expect` family.
- **Error context** — `Context<E>` attaches "what was being done" messages while preserving
  the original error type, via `context()` / `with_context()` and the `TRY_CTX` /
  `ASSIGN_OR_RETURN_CTX` macros.
- **`Diagnostic` protocol** — a non-virtual, user-implementable concept plus a generic
  `render()` that walks a context chain into `"outer: inner: root"` text.
- **Borrowing views** — `as_ref()` / `as_mut()` yield zero-copy `reference_wrapper` views.
- **Iteration** — `iter()` / `iter_mut()` / `begin()` / `end()` provide zero-or-one-element
  ranges usable with range-for and `std::ranges` algorithms.
- **Formatting interop** — optional `pjh_result/io.hpp` (`operator<<`) and
  `pjh_result/format.hpp` (`std::formatter`) for `Result`, `Option` and `Context`.
- **Rust `?`-style propagation** — `ASSIGN_OR_RETURN`, `TRY`, `TRY_CTX`,
  `ASSIGN_OR_RETURN_CTX`, plus `Failure<E>` for type-erased error return.
- **Strong exception guarantee** on assignment; `nothrow`-move invariant enforced at compile time.
- **Header-only**, no runtime dependencies.

## Requirements

- A C++20 compiler (concepts, `requires`-clauses).
- CMake ≥ 3.20 for the CMake target (manual include-only use needs neither CMake
  nor network access).

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
#include "pjh_result.hpp"

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
| Construct | `Ok(v)`, `Ok()` (when `T = void`), `Err(e)`, implicit `Result(Failure<E>)` |
| Inspect & query | `is_ok()`, `is_err()`, `is_moved()`, `is_ok_and(f)`, `is_err_and(f)`, `contains(v)`, `contains_err(e)`, `operator==` |
| Borrow | `as_ref()` → `Result<const T&, const E&>`, `as_mut()` → `Result<T&, E&>` |
| Extract | `unwrap()`, `unwrap_or(v)`, `unwrap_or_else(f)`, `unwrap_or_default()`, `expect(msg)`, `unwrap_err()`, `unwrap_err_or(e)`, `unwrap_err_or_else(f)`, `expect_err(msg)` |
| Transform | `map(f)`, `map_err(f)`, `map_or(def, f)`, `map_or_else(d, f)`, `inspect(f)`, `inspect_err(f)` |
| Chain & flatten | `and_then(f)`, `or_else(f)`, `and_with(r)`, `or_with(r)`, `flatten()` |
| Context | `context(msg)`, `with_context(f)` |
| Iterate | `iter()`, `iter_mut()`, `begin()`, `end()` |
| Transpose | `transpose()` → `Option<Result<U,E>>` ↔ `Result<Option<U>,E>` |
| To Option | `ok()`, `err()` (defined in `interop.hpp`) |
| Propagate | `ASSIGN_OR_RETURN(name, expr)`, `TRY(expr)`, `TRY_CTX(expr, msg)`, `ASSIGN_OR_RETURN_CTX(name, expr, msg)` (from `macros.hpp`) |

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
| Borrow | `as_ref()` → `Option<const T&>`, `as_mut()` → `Option<T&>` |
| Extract | `unwrap()`, `unwrap_or(v)`, `unwrap_or_else(f)`, `unwrap_or_default()`, `expect(msg)` |
| Transform | `map(f)`, `map_or(def, f)`, `map_or_else(d, f)`, `inspect(f)`, `filter(pred)` |
| Chain, flatten & zip | `and_then(f)`, `or_else(f)`, `and_with(o)`, `or_with(o)`, `flatten()`, `zip(o)`, `zip_with(o, f)`, `unzip()` |
| Exclusive or | `x_or(o)` |
| Mutate | `take()`, `take_if(pred)`, `replace(v)`, `insert(v)`, `get_or_insert(v)`, `get_or_insert_default()`, `get_or_insert_with(f)` |
| Iterate | `iter()`, `iter_mut()`, `begin()`, `end()` |
| To Result | `ok_or(e)`, `ok_or_else(f)` |
| Transpose | `transpose()` → `Result<Option<U>,E>` ↔ `Option<Result<U,E>>` |

`unwrap`/`expect` throw `pjh::result::bad_result_access` when called on `None`.

## Error context

`Context<E>` is a wrapper that stores the original error `E` unchanged and accumulates a
chain of human-readable "what was being done" messages around it. It is itself a valid
error type, so it composes with `Result` and with further context layers. The chain is a
flat, copyable vector — no recursive `optional<Context<E>>` and no complete-type games.

Messages are ordered **outermost-first**: the most recently attached layer comes first, so
rendering outer-to-inner is a forward walk.

```cpp
namespace res = pjh::result;
using CxResult = res::Result<Config, res::Context<IoError>>;

res::Result<Config, IoError> read_config(); // root error type
res::Result<void, IoError> validate(const Config &);

CxResult load()
{
    ASSIGN_OR_RETURN_CTX(cfg, read_config(), "parse config"); // IoError -> Context<IoError>
    TRY_CTX(validate(cfg), "validate config");
    return CxResult::Ok(cfg);
}

CxResult outer()
{
    ASSIGN_OR_RETURN_CTX(cfg, load(), "load app config"); // appends to the existing chain
    return CxResult::Ok(cfg);
}
```

On failure, `render(err)` produces `"load app config: parse config: <root message>"`;
`err.messages()` returns the chain (outermost-first) and `err.root_cause()` gives the
original `IoError`. `context(msg)` / `with_context(f)` are also available directly on a
`Result`: `context` materializes the message only on the `Err` path, so a successful
`Result` never pays for it.

### `Diagnostic` and `render`

Any error type can opt into uniform rendering by satisfying `Diagnostic` — a non-virtual
concept requiring a `const`-callable `message()` convertible to `std::string_view` and an
equality-comparable `kind()`:

```cpp
struct IoError
{
    std::string text;
    enum class Kind { open, read };
    Kind kind_;

    std::string_view message() const { return text; }
    Kind kind() const { return kind_; }
};

static_assert(res::Diagnostic<IoError>);
std::string s = res::render(err); // "open: <message>" when err is a Context<IoError>
```

A `Context<E>` structurally satisfies `Diagnostic` exactly when `E` does.

## Borrowing, iteration & formatting

**Borrowing views** — `as_ref()` / `as_mut()` return a `Result`/`Option` of
`std::reference_wrapper`s into the active branch, so the payload is neither copied nor
moved. They are deleted on rvalues to prevent dangling views.

```cpp
res::Result<std::string, int> r = res::Result<std::string, int>::Ok("hi");
auto view = r.as_mut();          // Result<std::string&, int&>
view.unwrap() += "!";            // writes through to r
```

**Iteration** — `iter()` / `iter_mut()` (and `begin()` / `end()`) yield a zero-or-one
element range, matching Rust's `Option::Iter` / `Result::Iter`:

```cpp
for (int &x : r.iter_mut())      // visits the value only when Ok
    x *= 2;

auto it = std::ranges::find(opt.iter(), 42); // ranges algorithms compose
```

**Stream output** — include `pjh_result/io.hpp` (not in the umbrella header):

```cpp
#include "pjh_result.hpp"
#include "pjh_result/io.hpp"

std::cout << res::Result<int, int>::Ok(3); // "Ok(3)"
std::cout << res::Option<int>::None();      // "None"
```

**`std::format`** — include `pjh_result/format.hpp`; it is a no-op unless `<format>` with
`__cpp_lib_format >= 201907L` is available, detectable via `PJH_RESULT_HAS_STD_FORMAT`:

```cpp
#include "pjh_result.hpp"
#include "pjh_result/format.hpp"

std::string s = std::format("{}", res::Result<int, int>::Ok(3)); // "Ok(3)"
```

`Context<E>` renders its full causal chain in both paths. A moved-from `Result` formats as
`Result(moved)` instead of throwing.

## Converting between the two

`Option → Result` is a member (`ok_or` / `ok_or_else`). The reverse direction is the
members `Result::ok()` / `Result::err()` plus the free functions `res::ok(r)` /
`res::err(r)`. These are defined in `pjh_result/interop.hpp` to avoid a circular
include; `result.hpp` only *declares* the members. The class headers alone leave the
return type `Option` incomplete and the definition absent, so code that calls them must
include the umbrella `pjh_result.hpp` (or `pjh_result/interop.hpp`):

```cpp
#include "pjh_result.hpp"

res::Result<int, std::string> r = /* ... */;
res::Option<int> maybe = r.ok();          // member form: Ok -> Some, Err -> None
res::Option<std::string> e = res::err(r); // free-function form: Err -> Some, Ok -> None
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

Context, `Diagnostic`, iteration, borrow views and formatting interop are exercised by the
test suite under [`tests/`](tests/).

## Building tests & examples

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

Toggle with `-DPJH_RESULT_BUILD_TESTS=ON/OFF` and `-DPJH_RESULT_BUILD_EXAMPLES=ON/OFF`
(both default to ON when this is the top-level project). Tests use
[doctest](https://github.com/doctest/doctest) v2.5.0, a tests-only optional
dependency fetched via CMake `FetchContent` at configure time — there is no
submodule or vendored copy, so network access is required during the first
configure when tests are enabled.

## License

MIT — see [LICENSE](LICENSE).
