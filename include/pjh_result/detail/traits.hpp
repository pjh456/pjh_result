/**
 * @file traits.hpp
 * @brief Type traits and concepts used to recognize and constrain `Result` types.
 */
#ifndef INCLUDE_PJH_RESULT_DETAIL_TRAITS_HPP
#define INCLUDE_PJH_RESULT_DETAIL_TRAITS_HPP

#include <type_traits>
#include <concepts>

namespace pjh::result::detail
{
    /// @brief Primary template; specialized (in result.hpp) for each `Result<T, E>` to
    ///        expose its `value_type` and `error_type`.
    template <typename>
    struct result_traits;

    /// @brief The success value type of a `Result`-like type `T`.
    template <typename T>
    using result_value_t =
        typename result_traits<std::remove_cvref_t<T>>::value_type;

    /// @brief The error value type of a `Result`-like type `T`.
    template <typename T>
    using result_error_t =
        typename result_traits<std::remove_cvref_t<T>>::error_type;

    /// @brief Satisfied when `T` is a `Result` (i.e. `result_traits` is specialized for it).
    template <typename T>
    concept ResultType =
        requires {
            typename result_traits<std::remove_cvref_t<T>>::value_type;
            typename result_traits<std::remove_cvref_t<T>>::error_type;
        };

    /// @brief Satisfied when `T` is not a `Result`.
    template <typename T>
    concept NotResult = !ResultType<T>;

    /// @brief Satisfied when `T` and `E` are distinct types (a `Result` forbids `T == E`).
    template <typename T, typename E>
    concept ValidResultTypes =
        !std::same_as<std::remove_cvref_t<T>,
                      std::remove_cvref_t<E>>;

    /// @brief Satisfied when `X` exposes a `value_type` member (heuristic for `Option`-like types).
    template <typename X>
    concept OptionType = requires {
        typename std::remove_cvref_t<X>::value_type;
    };
}

#endif // INCLUDE_PJH_RESULT_DETAIL_TRAITS_HPP
