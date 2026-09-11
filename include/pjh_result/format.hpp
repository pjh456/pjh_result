/**
 * @file format.hpp
 * @brief Optional `std::formatter` specializations for `Result`, `Option` and `Context`.
 *
 * This header is **not** part of the umbrella `pjh_result.hpp`: it is only usable
 * when the standard library provides `<format>` (`__cpp_lib_format >= 201907L`).
 * Include it explicitly before formatting library types:
 * @code
 * #include "pjh_result.hpp"
 * #include "pjh_result/format.hpp"
 * std::string s = std::format("{}", pjh::result::Result<int, int>::Ok(3)); // "Ok(3)"
 * @endcode
 *
 * When `<format>` is unavailable the header expands to nothing; check for the
 * @c PJH_RESULT_HAS_STD_FORMAT macro to detect availability.
 *
 * Only the `char` character type is supported, matching the library's
 * `std::string`-based error text.
 */
#ifndef INCLUDE_PJH_RESULT_FORMAT_HPP
#define INCLUDE_PJH_RESULT_FORMAT_HPP

#if defined(__has_include)
#    if __has_include(<format>)
#        include <format>
#        if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
#            define PJH_RESULT_HAS_STD_FORMAT 1
#        endif
#    endif
#endif

#ifdef PJH_RESULT_HAS_STD_FORMAT

#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>

#include "pjh_result.hpp"

namespace pjh::result::detail
{
    /**
     * @brief Satisfied when @p U has a usable `char` formatter.
     *
     * C++20 shim for the C++23 `std::formattable` concept: the standard concept is
     * only available with `__cpp_lib_format >= 202207L`, which GCC 13 and many
     * other C++20 implementations do not define.
     *
     * @tparam U the type to probe (cv/ref stripped before use)
     */
    template <typename U>
    concept formattable = requires(
        std::formatter<std::remove_cvref_t<U>, char> &f,
        const std::remove_cvref_t<U> &v,
        std::format_context &ctx)
    {
        f.format(v, ctx);
    };

    /// @brief Appends @p e to @p out, preferring a user `std::formatter<E>` over the
    ///        `Diagnostic` `render()` fallback.
    template <class E>
        requires(formattable<E> || Diagnostic<E>)
    void format_error_into(std::string &out, const E &e)
    {
        if constexpr (formattable<E>)
            std::format_to(std::back_inserter(out), "{}", e);
        else
            out += render(e);
    }
}

namespace std
{
    /**
     * @brief `std::formatter` for `pjh::result::Option<T>`; renders `Some(...)` / `None`.
     *
     * The element is formatted with its own default `std::formatter` and the whole
     * representation is passed through `std::formatter<std::string_view, char>`, so
     * string-style format specs (width, alignment, fill, precision) apply to the
     * complete `Some(...)` / `None` text.
     *
     * @tparam T the contained value type (non-void)
     */
    template <typename T>
        requires(!std::is_void_v<T>) && pjh::result::detail::formattable<T>
    struct formatter<pjh::result::Option<T>, char>
    {
        formatter<string_view, char> spec_;

        template <class Ctx>
        constexpr auto parse(Ctx &ctx)
        {
            return spec_.parse(ctx);
        }

        template <class Ctx>
        auto format(const pjh::result::Option<T> &o, Ctx &ctx) const
        {
            std::string buf = o.is_some() ? "Some(" : "None";
            if (o.is_some())
            {
                std::format_to(std::back_inserter(buf), "{}", o.unwrap());
                buf += ')';
            }
            return spec_.format(std::string_view(buf), ctx);
        }
    };

    /// @brief `std::formatter` for `pjh::result::Option<void>`; renders `Some()` / `None`.
    template <>
    struct formatter<pjh::result::Option<void>, char>
    {
        formatter<string_view, char> spec_;

        template <class Ctx>
        constexpr auto parse(Ctx &ctx)
        {
            return spec_.parse(ctx);
        }

        template <class Ctx>
        auto format(const pjh::result::Option<void> &o, Ctx &ctx) const
        {
            std::string buf = o.is_some() ? "Some()" : "None";
            return spec_.format(std::string_view(buf), ctx);
        }
    };

    /**
     * @brief `std::formatter` for `pjh::result::Context<E>`; renders the full
     *        outer-to-inner causal chain via `pjh::result::render`.
     *
     * @tparam E the root error type; must satisfy `pjh::result::Diagnostic`
     */
    template <typename E>
        requires pjh::result::Diagnostic<E>
    struct formatter<pjh::result::Context<E>, char>
    {
        formatter<string_view, char> spec_;

        template <class Ctx>
        constexpr auto parse(Ctx &ctx)
        {
            return spec_.parse(ctx);
        }

        template <class Ctx>
        auto format(const pjh::result::Context<E> &c, Ctx &ctx) const
        {
            std::string buf = pjh::result::render(c);
            return spec_.format(std::string_view(buf), ctx);
        }
    };

    /**
     * @brief `std::formatter` for `pjh::result::Result<T, E>` (non-void `T`).
     *
     * Renders `Ok(...)`, `Err(...)` or `Result(moved)`; formatting never throws,
     * even for a moved-from result. The error branch uses a user
     * `std::formatter<E>` when one exists, otherwise falls back to the
     * `Diagnostic` `render()` text.
     *
     * @tparam T the success value type (non-void); must be formattable
     * @tparam E the error type; must be formattable or satisfy `Diagnostic`
     */
    template <typename T, typename E>
        requires(!std::is_void_v<T>) && pjh::result::detail::formattable<T> &&
                (pjh::result::detail::formattable<E> || pjh::result::Diagnostic<E>)
    struct formatter<pjh::result::Result<T, E>, char>
    {
        formatter<string_view, char> spec_;

        template <class Ctx>
        constexpr auto parse(Ctx &ctx)
        {
            return spec_.parse(ctx);
        }

        template <class Ctx>
        auto format(const pjh::result::Result<T, E> &r, Ctx &ctx) const
        {
            std::string buf;
            if (r.is_moved()) // check before any value access: the union is dead
            {
                buf = "Result(moved)";
            }
            else if (r.is_ok())
            {
                buf = "Ok(";
                std::format_to(std::back_inserter(buf), "{}", r.unwrap());
                buf += ')';
            }
            else
            {
                buf = "Err(";
                pjh::result::detail::format_error_into(buf, r.unwrap_err());
                buf += ')';
            }
            return spec_.format(std::string_view(buf), ctx);
        }
    };

    /**
     * @brief `std::formatter` for `pjh::result::Result<void, E>`.
     *
     * Renders `Ok()`, `Err(...)` or `Result(moved)`.
     *
     * @tparam E the error type; must be formattable or satisfy `Diagnostic`
     */
    template <typename E>
        requires(pjh::result::detail::formattable<E> || pjh::result::Diagnostic<E>)
    struct formatter<pjh::result::Result<void, E>, char>
    {
        formatter<string_view, char> spec_;

        template <class Ctx>
        constexpr auto parse(Ctx &ctx)
        {
            return spec_.parse(ctx);
        }

        template <class Ctx>
        auto format(const pjh::result::Result<void, E> &r, Ctx &ctx) const
        {
            std::string buf;
            if (r.is_moved())
            {
                buf = "Result(moved)";
            }
            else if (r.is_ok())
            {
                buf = "Ok()";
            }
            else
            {
                buf = "Err(";
                pjh::result::detail::format_error_into(buf, r.unwrap_err());
                buf += ')';
            }
            return spec_.format(std::string_view(buf), ctx);
        }
    };
}

#endif // PJH_RESULT_HAS_STD_FORMAT

#endif // INCLUDE_PJH_RESULT_FORMAT_HPP
