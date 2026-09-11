/**
 * @file macros.hpp
 * @brief Rust `?`-style error-propagation macros for `Result`.
 */
#ifndef INCLUDE_PJH_RESULT_MACROS_HPP
#define INCLUDE_PJH_RESULT_MACROS_HPP

#include "pjh_result/result.hpp"

#define RESULT_CONCAT_INNER(a, b) a##b
#define RESULT_CONCAT(a, b) RESULT_CONCAT_INNER(a, b)
#define RESULT_UNIQUE_VAR(prefix) RESULT_CONCAT(prefix, __LINE__)

/**
 * @brief Rust `?`-operator analogue for a value-carrying `Result<T, E>`.
 *
 * Evaluates @p expr: if it is `Ok(v)`, declares `auto var_name = v`; if it is `Err(e)`,
 * the enclosing function immediately returns the error.
 *
 * @param var_name name bound to the unwrapped success value
 * @param expr an expression producing a `Result`
 */
#define ASSIGN_OR_RETURN(var_name, expr)                                                        \
    auto RESULT_UNIQUE_VAR(_res_) = (expr);                                                     \
    if (RESULT_UNIQUE_VAR(_res_).is_err())                                                      \
    {                                                                                           \
        return pjh::result::Failure{std::move(RESULT_UNIQUE_VAR(_res_).unwrap_err())};  \
    }                                                                                           \
    auto var_name = std::move(RESULT_UNIQUE_VAR(_res_).unwrap())

/**
 * @brief Rust `?`-operator analogue for a valueless `Result<void, E>`.
 *
 * Evaluates @p expr: if it is `Ok`, execution continues; if it is `Err(e)`, the enclosing
 * function immediately returns the error.
 *
 * @param expr an expression producing a `Result`
 */
#define TRY(expr)                                                                                   \
    do                                                                                              \
    {                                                                                               \
        auto RESULT_UNIQUE_VAR(_res_) = (expr);                                                     \
        if (RESULT_UNIQUE_VAR(_res_).is_err())                                                      \
        {                                                                                           \
            return pjh::result::Failure{std::move(RESULT_UNIQUE_VAR(_res_).unwrap_err())};  \
        }                                                                                           \
    } while (0)

/**
 * @brief Context-carrying variant of @ref TRY.
 *
 * Evaluates @p expr: if it is `Ok`, execution continues; if it is `Err(e)`, @p msg is
 * attached as one extra context layer (appended to the chain when `e` is already a
 * `Context`) and the enclosing function immediately returns the result.
 *
 * @p msg is evaluated only on the `Err` path, so no message is constructed for a
 * successful result. The enclosing function must return `Result<U, Context<E>>`, where
 * `E` is the error type of @p expr (`Result<U, E>` when `E` is already a `Context`); the
 * error type is inferred, never spelled by the macro.
 *
 * @param expr an expression producing a `Result`
 * @param msg  "what was being done" description, constructible into `std::string`
 */
#define TRY_CTX(expr, msg)                                                                          \
    do                                                                                              \
    {                                                                                               \
        auto RESULT_UNIQUE_VAR(_res_) = (expr);                                                     \
        if (RESULT_UNIQUE_VAR(_res_).is_err())                                                      \
        {                                                                                           \
            return pjh::result::Failure{                                                            \
                std::move(RESULT_UNIQUE_VAR(_res_)).context((msg)).unwrap_err()};                   \
        }                                                                                           \
    } while (0)

/**
 * @brief Context-carrying variant of @ref ASSIGN_OR_RETURN.
 *
 * Evaluates @p expr: if it is `Ok(v)`, declares `auto var_name = v`; if it is `Err(e)`,
 * attaches @p msg as one extra context layer (appended to the chain when `e` is already a
 * `Context`) and the enclosing function immediately returns the result.
 *
 * @p msg is evaluated only on the `Err` path. The enclosing function must return
 * `Result<U, Context<E>>` (or `Result<U, E>` when `E` is already a `Context`); the error
 * type is inferred, never spelled by the macro.
 *
 * @param var_name name bound to the unwrapped success value
 * @param expr an expression producing a `Result`
 * @param msg  "what was being done" description, constructible into `std::string`
 */
#define ASSIGN_OR_RETURN_CTX(var_name, expr, msg)                                                   \
    auto RESULT_UNIQUE_VAR(_res_) = (expr);                                                         \
    if (RESULT_UNIQUE_VAR(_res_).is_err())                                                          \
    {                                                                                               \
        return pjh::result::Failure{                                                                \
            std::move(RESULT_UNIQUE_VAR(_res_)).context((msg)).unwrap_err()};                       \
    }                                                                                               \
    auto var_name = std::move(RESULT_UNIQUE_VAR(_res_)).unwrap()

#endif // INCLUDE_PJH_RESULT_MACROS_HPP
