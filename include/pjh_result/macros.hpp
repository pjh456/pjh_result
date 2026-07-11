#ifndef INCLUDE_PJH_RESULT_MACROS_HPP
#define INCLUDE_PJH_RESULT_MACROS_HPP

#include <utility>

#include "pjh_result/result.hpp"

#define RESULT_CONCAT_INNER(a, b) a##b
#define RESULT_CONCAT(a, b) RESULT_CONCAT_INNER(a, b)
#define RESULT_UNIQUE_VAR(prefix) RESULT_CONCAT(prefix, __LINE__)

/**
 * @brief 仿 Rust `?` 运算符 (针对带返回值的 Result<T, E>)
 *
 * 作用：执行 expr，如果返回 Ok(v)，则声明 auto var_name = v；
 *      如果返回 Err(e)，则当前函数立即 return 错误。
 */
#define ASSIGN_OR_RETURN(var_name, expr)                                                        \
    auto RESULT_UNIQUE_VAR(_res_) = (expr);                                                     \
    if (RESULT_UNIQUE_VAR(_res_).is_err())                                                      \
    {                                                                                           \
        return pjh::result::utils::Failure{std::move(RESULT_UNIQUE_VAR(_res_).unwrap_err())};  \
    }                                                                                           \
    auto var_name = std::move(RESULT_UNIQUE_VAR(_res_).unwrap())

/**
 * @brief 仿 Rust `?` 运算符 (针对无返回值的 Result<void, E>)
 *
 * 作用：执行 expr，如果是 Ok 则继续往下走；如果是 Err，则当前函数立即 return 错误。
 */
#define TRY(expr)                                                                                   \
    do                                                                                              \
    {                                                                                               \
        auto RESULT_UNIQUE_VAR(_res_) = (expr);                                                     \
        if (RESULT_UNIQUE_VAR(_res_).is_err())                                                      \
        {                                                                                           \
            return pjh::result::utils::Failure{std::move(RESULT_UNIQUE_VAR(_res_).unwrap_err())};  \
        }                                                                                           \
    } while (0)

#endif // INCLUDE_PJH_RESULT_MACROS_HPP
