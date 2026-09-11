/**
 * @file traits.hpp
 * @brief Type traits and concepts used to recognize and constrain `Result` types.
 */
#ifndef INCLUDE_PJH_RESULT_DETAIL_TRAITS_HPP
#define INCLUDE_PJH_RESULT_DETAIL_TRAITS_HPP

#include <type_traits>
#include <concepts>
#include <utility>

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

    /// @brief Satisfied when `T` may be stored in a tagged union: `void` is allowed
    ///        (the valueless branch is represented by `Unit`), references are not
    ///        (a union cannot hold a reference member).
    template <typename T>
    concept Storable = !std::is_reference_v<T>;

    /// @brief Satisfied when `E` may be the error type of a `Result`: neither `void`
    ///        (there is no `Result<T, void>`) nor a reference.
    template <typename E>
    concept ValidErrorType = Storable<E> && (!std::is_void_v<E>);

    /// @brief Satisfied when `T` and `E` form a valid `Result`: a storable `T`, a
    ///        non-void non-reference `E`, and `T != E` (a `Result` forbids `T == E`).
    template <typename T, typename E>
    concept ValidResultTypes =
        Storable<T> && ValidErrorType<E> &&
        !std::same_as<std::remove_cvref_t<T>,
                      std::remove_cvref_t<E>>;

    /// @brief Primary template; specialized (in option.hpp) for each `Option<T>` to
    ///        expose its `value_type`. Mirrors `result_traits`.
    template <typename>
    struct option_traits;

    /// @brief Satisfied when `X` is this library's `Option` (i.e. `option_traits` is
    ///        specialized for it). A stray `value_type` member (e.g. on `std::string`,
    ///        `std::vector` or a detail iterator) does not qualify.
    template <typename X>
    concept OptionType = requires {
        typename option_traits<std::remove_cvref_t<X>>::value_type;
    };

    /// @brief Satisfied when `X` (ignoring cv/ref) is a `std::pair<A, B>` specialization.
    template <typename X>
    struct is_std_pair : std::false_type
    {
    };
    template <typename A, typename B>
    struct is_std_pair<std::pair<A, B>> : std::true_type
    {
    };

    /// @brief Satisfied when `X` is a `std::pair<A, B>` with non-reference element types.
    ///        A bare pair-like type that merely exposes `first_type` / `second_type`, and
    ///        a `std::pair<A &, B &>` (which an `Option` cannot store), are rejected.
    template <typename X>
    concept PairType =
        requires {
            typename std::remove_cvref_t<X>::first_type;
            typename std::remove_cvref_t<X>::second_type;
        } &&
        is_std_pair<std::remove_cvref_t<X>>::value &&
        (!std::is_reference_v<typename std::remove_cvref_t<X>::first_type>) &&
        (!std::is_reference_v<typename std::remove_cvref_t<X>::second_type>);
}

#endif // INCLUDE_PJH_RESULT_DETAIL_TRAITS_HPP
