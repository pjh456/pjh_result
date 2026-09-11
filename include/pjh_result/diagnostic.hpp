/**
 * @file diagnostic.hpp
 * @brief User-implementable `Diagnostic` protocol and a generic error renderer.
 *
 * A type opts in by exposing a `const`-callable `message()` convertible to
 * `std::string_view` and a `const`-callable `kind()` whose return type is
 * equality-comparable. No base class, registration or virtual function is required.
 * `pjh::result::render` walks an error and, when it is a `Context<E>` chain,
 * produces `"outer: inner: root"` text.
 */
#ifndef INCLUDE_PJH_RESULT_DIAGNOSTIC_HPP
#define INCLUDE_PJH_RESULT_DIAGNOSTIC_HPP

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>

#include "pjh_result/context.hpp"

namespace pjh::result
{
    /**
     * @brief An error exposing a human-readable message and a stable, comparable kind tag.
     *
     * `message()` must return something convertible to `std::string_view`; implementations
     * should return a view into stable storage (e.g. a member string) rather than a
     * temporary `std::string`, since binding the result to a `std::string_view` of a
     * temporary dangles.
     *
     * `kind()` must return an equality-comparable value (typically a scoped enum). The
     * concept only guarantees that values of one error's kind compare with each other;
     * generic code must not compare kinds across different error types. Rendering uses
     * `message()` only.
     */
    template <class E>
    concept Diagnostic = requires(const E &e) {
        { e.message() } -> std::convertible_to<std::string_view>;
        { e.kind() } -> std::equality_comparable;
    };

    namespace detail
    {
        /// @brief Appends @p e to @p out, walking a `Context` chain outer-to-inner.
        template <class E>
        void append_diagnostic(std::string &out, const E &e)
        {
            if constexpr (is_context_v<E>)
            {
                for (const auto &m : e.messages()) // outermost-first
                {
                    out += m;
                    out += ": ";
                }
                append_diagnostic(out, e.root_cause());
            }
            else
            {
                out += std::string_view(e.message());
            }
        }
    }

    /**
     * @brief Renders any `Diagnostic` (or `Context` thereof) to `"outer: inner: root"`.
     *
     * The full context chain is the responsibility of this function; a
     * `Context<E>::message()` only exposes the outermost layer.
     *
     * @tparam E a type satisfying @ref Diagnostic after stripping cv/ref
     * @param e the error to render
     * @return owning text; an empty chain yields just the root message
     */
    template <class E>
        requires Diagnostic<std::remove_cvref_t<E>>
    [[nodiscard]] std::string render(const E &e)
    {
        std::string out;
        detail::append_diagnostic(out, e);
        return out;
    }
}

#endif // INCLUDE_PJH_RESULT_DIAGNOSTIC_HPP
