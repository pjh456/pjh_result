#ifndef INCLUDE_PJH_RESULT_DETAIL_TRAITS_HPP
#define INCLUDE_PJH_RESULT_DETAIL_TRAITS_HPP

#include <type_traits>
#include <concepts>

namespace pjh::result::utils::result_helper
{
    template <typename>
    struct result_traits;

    template <typename T>
    using result_value_t =
        typename result_traits<std::remove_cvref_t<T>>::value_type;

    template <typename T>
    using result_error_t =
        typename result_traits<std::remove_cvref_t<T>>::error_type;

    template <typename T>
    concept ResultType =
        requires {
            typename result_traits<std::remove_cvref_t<T>>::value_type;
            typename result_traits<std::remove_cvref_t<T>>::error_type;
        };

    template <typename T>
    concept NotResult = !ResultType<T>;

    template <typename T, typename E>
    concept ValidResultTypes =
        !std::same_as<std::remove_cvref_t<T>,
                      std::remove_cvref_t<E>>;
}

#endif // INCLUDE_PJH_RESULT_DETAIL_TRAITS_HPP
