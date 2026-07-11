/**
 * @file option.hpp
 * @brief Rust-style `Option<T>` backed by hand-written tagged storage.
 */
#ifndef INCLUDE_PJH_RESULT_OPTION_HPP
#define INCLUDE_PJH_RESULT_OPTION_HPP

#include <concepts>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "pjh_result/result.hpp"

namespace pjh::result
{
    /**
     * @brief An optional value monad.
     *
     * A Rust-like `Option<T>` that holds either a value (`Some`) or nothing (`None`).
     *
     * Storage is a hand-written value/empty union plus a `bool` flag rather than
     * `std::optional`, matching `Result`'s controlled, no-hidden-state design.
     *
     * `T = void` is supported: `Some()` carries no value (a mere presence flag) and
     * `unwrap()` returns `void`.
     *
     * @tparam T contained value type (may be `void`)
     *
     * @pre The move constructor of `T` (when non-void) must be `noexcept` (enforced by the
     *      in-class `static_assert`), so assignment can destroy-then-nothrow-move and never
     *      leave the object in an invalid state.
     * @note The return value must not be ignored (`[[nodiscard]]`).
     */
    template <typename T>
    class [[nodiscard]] Option
    {
    private:
        /// @brief Actual storage type; degrades to `Unit` when `T = void`.
        using StoredT = std::conditional_t<std::is_void_v<T>, Unit, T>;

        static_assert(std::is_nothrow_move_constructible_v<StoredT>,
                      "pjh::result::Option requires T to be nothrow move constructible");

        bool has_value_;
        union
        {
            StoredT value_;
        };

        struct some_t
        {
        };
        struct none_t
        {
        };

        /// @brief In-place constructs the Some branch (value forwarded from `a...`,
        ///        value-initialized when no argument is given).
        template <typename... A>
            requires std::constructible_from<StoredT, A &&...>
        explicit Option(some_t, A &&...a) noexcept(std::is_nothrow_constructible_v<StoredT, A &&...>)
            : has_value_(true), value_(std::forward<A>(a)...)
        {
        }

        /// @brief Constructs the empty (None) state.
        explicit Option(none_t) noexcept : has_value_(false) {}

        /// @brief Destroys the contained value if present; a no-op for trivially destructible types.
        void destroy_() noexcept
        {
            if (has_value_)
            {
                if constexpr (!std::is_trivially_destructible_v<StoredT>)
                    value_.~StoredT();
            }
        }

        /// @brief Nothrow move-constructs this object's state from an rvalue `Option`
        ///        (assumes this object's storage is empty / already destroyed).
        void construct_from_(Option &&o) noexcept
        {
            has_value_ = o.has_value_;
            if (has_value_)
                ::new (static_cast<void *>(std::addressof(value_))) StoredT(std::move(o.value_));
        }

    public:
        /**
         * @brief Constructs `Some(val)`. Available only when `T` is non-void.
         *
         * @tparam U argument type used to construct `T`
         * @param val the contained value
         * @return an `Option` in the Some state
         */
        template <typename U>
            requires(!std::is_void_v<T>) && std::constructible_from<StoredT, U &&>
        static Option Some(U &&val) noexcept(std::is_nothrow_constructible_v<StoredT, U &&>)
        {
            return Option(some_t{}, std::forward<U>(val));
        }

        /**
         * @brief Constructs `Some()`. Available only when `T` is `void`.
         *
         * @return an `Option` in the Some state
         */
        static Option Some() noexcept
            requires std::is_void_v<T>
        {
            return Option(some_t{});
        }

        /**
         * @brief Constructs `None`.
         *
         * @return an `Option` in the None state
         */
        static Option None() noexcept
        {
            return Option(none_t{});
        }

    public:
        /// @brief Copy constructor: copies the contained value if present.
        Option(const Option &o)
            requires std::copy_constructible<StoredT>
            : has_value_(o.has_value_)
        {
            if (has_value_)
                ::new (static_cast<void *>(std::addressof(value_))) StoredT(o.value_);
        }

        /// @brief Move constructor: nothrow-moves the contained value if present.
        Option(Option &&o) noexcept : has_value_(o.has_value_)
        {
            if (has_value_)
                ::new (static_cast<void *>(std::addressof(value_))) StoredT(std::move(o.value_));
        }

        /**
         * @brief Copy assignment (strong exception guarantee).
         *
         * First copy-constructs a temporary (if this step throws, `*this` is left unchanged),
         * then destroys the old value and nothrow move-constructs from the temporary.
         */
        Option &operator=(const Option &o)
            requires std::copy_constructible<StoredT>
        {
            if (this != std::addressof(o))
            {
                Option tmp(o);
                destroy_();
                construct_from_(std::move(tmp));
            }
            return *this;
        }

        /// @brief Move assignment: destroys the old value, then nothrow move-constructs.
        Option &operator=(Option &&o) noexcept
        {
            if (this != std::addressof(o))
            {
                destroy_();
                construct_from_(std::move(o));
            }
            return *this;
        }

        /// @brief Destructor: destroys the contained value if present.
        ~Option() { destroy_(); }

    public:
        /// @brief Whether the option currently holds a value.
        bool is_some() const noexcept { return has_value_; }
        /// @brief Whether the option is currently empty.
        bool is_none() const noexcept { return !has_value_; }

    public:
        /**
         * @brief Unwraps the contained value; throws if None. Available only when `T` is non-void.
         *
         * @throws bad_result_access when currently None
         * @return reference to the contained value
         */
        [[nodiscard]] StoredT &unwrap() &
            requires(!std::is_void_v<T>)
        {
            if (!has_value_)
                throw bad_result_access("Option::unwrap() called on None");
            return value_;
        }

        /**
         * @brief Unwraps the contained value; throws if None. Available only when `T` is non-void.
         *
         * @throws bad_result_access when currently None
         * @return const reference to the contained value
         */
        [[nodiscard]] const StoredT &unwrap() const &
            requires(!std::is_void_v<T>)
        {
            if (!has_value_)
                throw bad_result_access("Option::unwrap() called on None");
            return value_;
        }

        /**
         * @brief Unwraps and moves out the contained value; throws if None. Available only when
         *        `T` is non-void.
         *
         * @throws bad_result_access when currently None
         * @return the moved-out value
         */
        [[nodiscard]] StoredT unwrap() &&
            requires(!std::is_void_v<T>)
        {
            if (!has_value_)
                throw bad_result_access("Option::unwrap() called on None");
            return std::move(value_);
        }

        /**
         * @brief Asserts the option is Some; throws if None. Available only when `T` is `void`.
         *
         * @throws bad_result_access when currently None
         */
        void unwrap() const
            requires std::is_void_v<T>
        {
            if (!has_value_)
                throw bad_result_access("Option<void>::unwrap() called on None");
        }
    };
}

#endif // INCLUDE_PJH_RESULT_OPTION_HPP
