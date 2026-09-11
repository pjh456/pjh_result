/**
 * @file io.hpp
 * @brief Optional `operator<<` overloads for `Result`, `Option` and `Context`.
 *
 * This header is **not** part of the umbrella `pjh_result.hpp`; consumers that want
 * stream output include it explicitly:
 * @code
 * #include "pjh_result.hpp"
 * #include "pjh_result/io.hpp"
 * std::ostringstream os;
 * os << pjh::result::Result<int, int>::Ok(3); // "Ok(3)"
 * @endcode
 *
 * Unlike `pjh_result/format.hpp` this header needs no feature guard: it only uses
 * `<ostream>` / `<sstream>`. The rendered representation matches the
 * `std::formatter` specialization for scalar elements; elements are streamed with
 * their own `operator<<`, so a `std::string` error is printed unquoted while
 * `std::format` would quote it.
 */
#ifndef INCLUDE_PJH_RESULT_IO_HPP
#define INCLUDE_PJH_RESULT_IO_HPP

#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>

#include "pjh_result.hpp"

namespace pjh::result
{
    namespace detail
    {
        /**
         * @brief Satisfied when @p U (non-void) can be inserted into an `std::ostream`.
         *
         * The `!std::is_void_v` guard is evaluated first so a `void` element never
         * forms an invalid `const void &` reference.
         */
        template <class U>
        concept streamable = !std::is_void_v<std::remove_cvref_t<U>> &&
                             requires(std::ostream &os, const std::remove_cvref_t<U> &u) { os << u; };

        /// @brief Appends @p e to @p out, preferring `operator<<` over the `Diagnostic`
        ///        `render()` fallback.
        template <class E>
            requires(streamable<E> || Diagnostic<E>)
        void append_stream_error(std::ostringstream &out, const E &e)
        {
            if constexpr (streamable<E>)
                out << e;
            else
                out << render(e);
        }
    }

    /**
     * @brief Streams `Result<T, E>` as `Ok(...)` / `Err(...)` / `Result(moved)`.
     *
     * A moved-from result prints `Result(moved)` instead of throwing. The error is
     * streamed with its own `operator<<` when available, otherwise with the
     * `Diagnostic` `render()` text.
     *
     * @param os the output stream
     * @param r the result to render
     * @return @p os
     */
    template <typename T, typename E>
        requires(std::is_void_v<T> || detail::streamable<T>) &&
                (detail::streamable<E> || Diagnostic<E>)
    std::ostream &operator<<(std::ostream &os, const Result<T, E> &r)
    {
        std::ostringstream buf;
        if (r.is_moved()) // check before any value access: the union is dead
        {
            buf << "Result(moved)";
        }
        else if (r.is_ok())
        {
            buf << "Ok(";
            if constexpr (!std::is_void_v<T>)
                buf << r.unwrap();
            buf << ')';
        }
        else
        {
            buf << "Err(";
            detail::append_stream_error(buf, r.unwrap_err());
            buf << ')';
        }
        return os << buf.str();
    }

    /**
     * @brief Streams `Option<T>` as `Some(...)` / `None`.
     *
     * @param os the output stream
     * @param o the option to render
     * @return @p os
     */
    template <typename T>
        requires(std::is_void_v<T> || detail::streamable<T>)
    std::ostream &operator<<(std::ostream &os, const Option<T> &o)
    {
        std::ostringstream buf;
        if (o.is_some())
        {
            buf << "Some(";
            if constexpr (!std::is_void_v<T>)
                buf << o.unwrap();
            buf << ')';
        }
        else
        {
            buf << "None";
        }
        return os << buf.str();
    }

    /**
     * @brief Streams `Context<E>` as the full outer-to-inner causal chain.
     *
     * @param os the output stream
     * @param c the context to render
     * @return @p os
     */
    template <typename E>
        requires Diagnostic<E>
    std::ostream &operator<<(std::ostream &os, const Context<E> &c)
    {
        return os << render(c);
    }
}

#endif // INCLUDE_PJH_RESULT_IO_HPP
