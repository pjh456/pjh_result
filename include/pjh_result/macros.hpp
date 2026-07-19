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

#endif // INCLUDE_PJH_RESULT_MACROS_HPP
