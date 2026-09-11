/**
 * @file interop.hpp
 * @brief `Result` -> `Option` conversions: `Result::ok()` / `Result::err()` members and
 *        their free-function counterparts.
 *
 * These live outside both class headers to avoid a circular include: `option.hpp`
 * already depends on `result.hpp` (for `ok_or`), so the `Result` -> `Option` direction
 * is declared in `result.hpp` (which forward-declares `Option`) and defined here, where
 * both types are complete. The free functions `ok()` / `err()` provide the historical
 * call syntax alongside the members.
 */
#ifndef INCLUDE_PJH_RESULT_INTEROP_HPP
#define INCLUDE_PJH_RESULT_INTEROP_HPP

#include <type_traits>
#include <utility>

#include "pjh_result/option.hpp"
#include "pjh_result/result.hpp"

namespace pjh::result
{
    /// @brief Ok -> `Some(value)` (or `Some()` when `T = void`), Err -> `None`.
    /// @throws bad_result_access when `*this` is in the Moved state
    template <typename T, typename E>
        requires detail::ValidResultTypes<T, E> && detail::NotResult<E>
    Option<T> Result<T, E>::ok() const &
    {
        require_not_moved_();
        if (!is_ok())
            return Option<T>::None();
        if constexpr (std::is_void_v<T>)
            return Option<void>::Some();
        else
            return Option<T>::Some(ok_);
    }

    /// @brief Rvalue overload: moves the success value out and leaves `*this` Moved.
    /// @throws bad_result_access when `*this` is in the Moved state
    template <typename T, typename E>
        requires detail::ValidResultTypes<T, E> && detail::NotResult<E>
    Option<T> Result<T, E>::ok() &&
    {
        require_not_moved_();
        if (is_ok())
        {
            if constexpr (std::is_void_v<T>)
            {
                destroy_();
                tag_ = detail::Tag::Moved;
                return Option<void>::Some();
            }
            else
            {
                T tmp = std::move(ok_);
                destroy_();
                tag_ = detail::Tag::Moved;
                return Option<T>::Some(std::move(tmp));
            }
        }
        destroy_();
        tag_ = detail::Tag::Moved;
        return Option<T>::None();
    }

    /// @brief Err -> `Some(error)`, Ok -> `None`.
    /// @throws bad_result_access when `*this` is in the Moved state
    template <typename T, typename E>
        requires detail::ValidResultTypes<T, E> && detail::NotResult<E>
    Option<E> Result<T, E>::err() const &
    {
        require_not_moved_();
        if (!is_err())
            return Option<E>::None();
        return Option<E>::Some(err_);
    }

    /// @brief Rvalue overload: moves the error out and leaves `*this` Moved.
    /// @throws bad_result_access when `*this` is in the Moved state
    template <typename T, typename E>
        requires detail::ValidResultTypes<T, E> && detail::NotResult<E>
    Option<E> Result<T, E>::err() &&
    {
        require_not_moved_();
        if (is_err())
        {
            E tmp = std::move(err_);
            destroy_();
            tag_ = detail::Tag::Moved;
            return Option<E>::Some(std::move(tmp));
        }
        destroy_();
        tag_ = detail::Tag::Moved;
        return Option<E>::None();
    }

    /**
     * @brief Converts a `Result<T, E>` to `Option<T>`, discarding any error.
     *
     * Ok becomes `Some` (`Some()` when `T = void`); Err becomes `None`.
     *
     * @tparam T success value type
     * @tparam E error value type
     * @param r the result to convert
     * @return `Some(value)` if @p r is Ok, otherwise `None`
     */
    template <typename T, typename E>
    [[nodiscard]] Option<T> ok(const Result<T, E> &r)
    {
        if (r.is_ok())
        {
            if constexpr (std::is_void_v<T>)
                return Option<void>::Some();
            else
                return Option<T>::Some(r.unwrap());
        }
        return Option<T>::None();
    }

    /**
     * @brief Converts a `Result<T, E>` to `Option<T>`, moving the value out.
     *
     * @tparam T success value type
     * @tparam E error value type
     * @param r the result to convert (consumed)
     * @return `Some(value)` if @p r is Ok, otherwise `None`
     */
    template <typename T, typename E>
    [[nodiscard]] Option<T> ok(Result<T, E> &&r)
    {
        if (r.is_ok())
        {
            if constexpr (std::is_void_v<T>)
                return Option<void>::Some();
            else
                return Option<T>::Some(std::move(r).unwrap());
        }
        return Option<T>::None();
    }

    /**
     * @brief Converts a `Result<T, E>` to `Option<E>`, discarding any success value.
     *
     * Err becomes `Some`; Ok becomes `None`.
     *
     * @tparam T success value type
     * @tparam E error value type
     * @param r the result to convert
     * @return `Some(error)` if @p r is Err, otherwise `None`
     */
    template <typename T, typename E>
    [[nodiscard]] Option<E> err(const Result<T, E> &r)
    {
        if (r.is_err())
            return Option<E>::Some(r.unwrap_err());
        return Option<E>::None();
    }

    /**
     * @brief Converts a `Result<T, E>` to `Option<E>`, moving the error out.
     *
     * @tparam T success value type
     * @tparam E error value type
     * @param r the result to convert (consumed)
     * @return `Some(error)` if @p r is Err, otherwise `None`
     */
    template <typename T, typename E>
    [[nodiscard]] Option<E> err(Result<T, E> &&r)
    {
        if (r.is_err())
            return Option<E>::Some(std::move(r).unwrap_err());
        return Option<E>::None();
    }
}

#endif // INCLUDE_PJH_RESULT_INTEROP_HPP
