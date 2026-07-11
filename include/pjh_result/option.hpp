/**
 * @file option.hpp
 * @brief Rust-style `Option<T>` backed by hand-written tagged storage.
 */
#ifndef INCLUDE_PJH_RESULT_OPTION_HPP
#define INCLUDE_PJH_RESULT_OPTION_HPP

#include <concepts>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

#include "pjh_result/result.hpp"

namespace pjh::result
{
    template <typename T>
    class Option;

    namespace detail
    {
        /// @brief Trait detecting whether a type is an `Option` specialization.
        template <typename>
        struct is_option : std::false_type
        {
        };
        template <typename U>
        struct is_option<Option<U>> : std::true_type
        {
        };

        /// @brief Satisfied when `X` (ignoring cvref) is an `Option`.
        template <typename X>
        concept OptionType = is_option<std::remove_cvref_t<X>>::value;
    }

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

        /**
         * @brief Whether the option is Some and the value satisfies @p f.
         *
         * When `T = void`, @p f is invoked with no argument.
         *
         * @tparam F predicate on the value (or nullary when `T = void`)
         * @param f the predicate
         * @return `is_some() && bool(f(...))`
         */
        template <typename F>
            requires detail::MapCallable<F, T>
        [[nodiscard]] bool is_some_and(F &&f) const
        {
            if (!has_value_)
                return false;
            if constexpr (std::is_void_v<T>)
                return static_cast<bool>(std::invoke(f));
            else
                return static_cast<bool>(std::invoke(f, value_));
        }

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

        /**
         * @brief Unwraps the value, throwing with a custom message if None.
         *        Available only when `T` is non-void.
         *
         * @param msg message carried by the thrown exception
         * @throws bad_result_access when currently None
         * @return reference to the contained value
         */
        [[nodiscard]] StoredT &expect(const std::string &msg) &
            requires(!std::is_void_v<T>)
        {
            if (!has_value_)
                throw bad_result_access(msg);
            return value_;
        }

        /**
         * @brief Unwraps the value (const overload), throwing @p msg if None.
         *        Available only when `T` is non-void.
         *
         * @param msg message carried by the thrown exception
         * @throws bad_result_access when currently None
         * @return const reference to the contained value
         */
        [[nodiscard]] const StoredT &expect(const std::string &msg) const &
            requires(!std::is_void_v<T>)
        {
            if (!has_value_)
                throw bad_result_access(msg);
            return value_;
        }

        /**
         * @brief Unwraps and moves out the value, throwing @p msg if None.
         *        Available only when `T` is non-void.
         *
         * @param msg message carried by the thrown exception
         * @throws bad_result_access when currently None
         * @return the moved-out value
         */
        [[nodiscard]] StoredT expect(const std::string &msg) &&
            requires(!std::is_void_v<T>)
        {
            if (!has_value_)
                throw bad_result_access(msg);
            return std::move(value_);
        }

        /**
         * @brief Asserts the option is Some, throwing @p msg if None.
         *        Available only when `T` is `void`.
         *
         * @param msg message carried by the thrown exception
         * @throws bad_result_access when currently None
         */
        void expect(const std::string &msg) const
            requires std::is_void_v<T>
        {
            if (!has_value_)
                throw bad_result_access(msg);
        }

        /**
         * @brief Unwraps the value, or returns the given default if None.
         *        Available only when `T` is non-void.
         *
         * @param val the default returned when None
         * @return the contained value, or @p val
         */
        [[nodiscard]] StoredT unwrap_or(StoredT val) const
            requires(!std::is_void_v<T>)
        {
            return has_value_ ? value_ : std::move(val);
        }

        /**
         * @brief Unwraps the value, or computes a fallback if None.
         *        Available only when `T` is non-void.
         *
         * @tparam F nullary callable returning a value convertible to `T`
         * @param f fallback producer invoked when None
         * @return the contained value, or `f()`
         */
        template <typename F>
            requires(!std::is_void_v<T>) && std::invocable<F> &&
                    std::convertible_to<std::invoke_result_t<F>, T>
        [[nodiscard]] T unwrap_or_else(F &&f) const
        {
            if (has_value_)
                return value_;
            return static_cast<T>(std::invoke(f));
        }

        /**
         * @brief Unwraps the value, or returns a default-constructed `T` if None.
         *        Available only when `T` is non-void and default-initializable.
         *
         * @return the contained value, or `T{}`
         */
        [[nodiscard]] T unwrap_or_default() const
            requires(!std::is_void_v<T>) && std::default_initializable<T>
        {
            if (has_value_)
                return value_;
            return T{};
        }

    public:
        /**
         * @brief Transforms the contained value (Map).
         *
         * On Some, applies `f` (calls `f()` when `T = void`) and returns `Some(f(...))`;
         * on None, returns `None`.
         *
         * @tparam F the transform callable
         * @param f callable applied to the value, returning `U`
         * @return `Option<U>`
         */
        template <typename F>
            requires detail::MapCallable<F, T>
        [[nodiscard]] auto map(F &&f) const -> Option<detail::map_result_t<F, T>>
        {
            using U = detail::map_result_t<F, T>;
            if (has_value_)
            {
                if constexpr (std::is_void_v<U>)
                {
                    if constexpr (std::is_void_v<T>)
                        std::invoke(f);
                    else
                        std::invoke(f, value_);
                    return Option<void>::Some();
                }
                else
                {
                    if constexpr (std::is_void_v<T>)
                        return Option<U>::Some(std::invoke(f));
                    else
                        return Option<U>::Some(std::invoke(f, value_));
                }
            }
            return Option<U>::None();
        }

        /**
         * @brief Returns `f(value)` if Some, otherwise the provided default.
         *
         * Collapses to a plain value `U`. When `T = void`, `f()` is called.
         *
         * @tparam F callable producing `U` (or nullary when `T = void`)
         * @param def value returned when None
         * @param f transform applied to the value
         * @return `f(...)` if Some, otherwise @p def
         */
        template <typename F>
            requires detail::MapCallable<F, T> && (!std::is_void_v<detail::map_result_t<F, T>>)
        [[nodiscard]] detail::map_result_t<F, T> map_or(detail::map_result_t<F, T> def, F &&f) const
        {
            if (has_value_)
            {
                if constexpr (std::is_void_v<T>)
                    return std::invoke(f);
                else
                    return std::invoke(f, value_);
            }
            return def;
        }

        /**
         * @brief Returns `f(value)` if Some, otherwise `d()`.
         *
         * Collapses to a plain value `U`. When `T = void`, `f()` is called.
         *
         * @tparam D nullary callable producing `U`
         * @tparam F callable producing `U` from the value (or nullary when `T = void`)
         * @param d fallback producer invoked when None
         * @param f transform applied to the value
         * @return `f(...)` if Some, otherwise `d()`
         */
        template <typename D, typename F>
            requires detail::MapCallable<F, T> && std::invocable<D> &&
                     (!std::is_void_v<detail::map_result_t<F, T>>) &&
                     std::convertible_to<std::invoke_result_t<D>, detail::map_result_t<F, T>>
        [[nodiscard]] detail::map_result_t<F, T> map_or_else(D &&d, F &&f) const
        {
            using U = detail::map_result_t<F, T>;
            if (has_value_)
            {
                if constexpr (std::is_void_v<T>)
                    return std::invoke(f);
                else
                    return std::invoke(f, value_);
            }
            return static_cast<U>(std::invoke(d));
        }

        /**
         * @brief Invokes @p f on the value if Some, then returns `*this` unchanged.
         *
         * When `T = void`, `f()` is called.
         *
         * @tparam F callable observing the value (or nullary when `T = void`)
         * @param f the observer
         * @return const reference to `*this`
         * @warning Returns a reference to `*this`; do not call on a temporary and keep the result.
         */
        template <typename F>
            requires detail::MapCallable<F, T>
        const Option &inspect(F &&f) const &
        {
            if (has_value_)
            {
                if constexpr (std::is_void_v<T>)
                    std::invoke(f);
                else
                    std::invoke(f, value_);
            }
            return *this;
        }

    public:
        /**
         * @brief Chains an option-returning operation (FlatMap / AndThen).
         *
         * On Some, invokes `f` (`f()` when `T = void`, otherwise `f(value)`; `f` must return an
         * `Option`); on None, short-circuits and returns `None`.
         *
         * @tparam F callable returning an `Option`
         * @param f the follow-up operation
         * @return the return type of `f` (an `Option`)
         */
        template <typename F>
            requires detail::OptionType<detail::cref_result_t<F, T>>
        auto and_then(F &&f) const & -> detail::cref_result_t<F, T>
        {
            using Ret = detail::cref_result_t<F, T>;
            if (has_value_)
            {
                if constexpr (std::is_void_v<T>)
                    return std::invoke(f);
                else
                    return std::invoke(f, value_);
            }
            return Ret::None();
        }

        /**
         * @brief Chains an option-returning operation (FlatMap / AndThen), rvalue/move overload.
         *
         * @tparam F callable returning an `Option`
         * @param f the follow-up operation
         * @return the return type of `f` (an `Option`)
         */
        template <typename F>
            requires detail::OptionType<detail::value_result_t<F, T>>
        auto and_then(F &&f) && -> detail::value_result_t<F, T>
        {
            using Ret = detail::value_result_t<F, T>;
            if (has_value_)
            {
                if constexpr (std::is_void_v<T>)
                    return std::invoke(f);
                else
                    return std::invoke(f, std::move(value_));
            }
            return Ret::None();
        }

        /**
         * @brief Returns `*this` if Some, otherwise the option produced by `f()`.
         *
         * @tparam F nullary callable returning an `Option`
         * @param f the recovery producer
         * @return `*this` (as the result type) if Some, otherwise `f()`
         */
        template <typename F>
            requires detail::OptionType<std::invoke_result_t<F>>
        auto or_else(F &&f) const & -> std::invoke_result_t<F>
        {
            using Ret = std::invoke_result_t<F>;
            if (!has_value_)
                return std::invoke(f);
            if constexpr (std::is_void_v<T>)
                return Ret::Some();
            else
                return Ret::Some(value_);
        }

        /**
         * @brief Returns `*this` if Some, otherwise `f()`, rvalue/move overload.
         *
         * @tparam F nullary callable returning an `Option`
         * @param f the recovery producer
         * @return `*this` (as the result type) if Some, otherwise `f()`
         */
        template <typename F>
            requires detail::OptionType<std::invoke_result_t<F>>
        auto or_else(F &&f) && -> std::invoke_result_t<F>
        {
            using Ret = std::invoke_result_t<F>;
            if (!has_value_)
                return std::invoke(f);
            if constexpr (std::is_void_v<T>)
                return Ret::Some();
            else
                return Ret::Some(std::move(value_));
        }

        /**
         * @brief Keeps the value only if it satisfies @p pred, otherwise yields `None`.
         *
         * When `T = void`, `pred()` is called.
         *
         * @tparam F predicate on the value (or nullary when `T = void`)
         * @param pred the predicate
         * @return `Some(value)` if present and @p pred holds, otherwise `None`
         */
        template <typename F>
            requires detail::MapCallable<F, T> && std::copy_constructible<StoredT>
        [[nodiscard]] Option filter(F &&pred) const &
        {
            if (has_value_)
            {
                if constexpr (std::is_void_v<T>)
                {
                    if (std::invoke(pred))
                        return Option::Some();
                }
                else
                {
                    if (std::invoke(pred, value_))
                        return Option::Some(value_);
                }
            }
            return Option::None();
        }

        /**
         * @brief Keeps the value only if it satisfies @p pred, rvalue/move overload.
         *
         * @tparam F predicate on the value (or nullary when `T = void`)
         * @param pred the predicate
         * @return `Some(value)` if present and @p pred holds, otherwise `None`
         */
        template <typename F>
            requires detail::MapCallable<F, T>
        [[nodiscard]] Option filter(F &&pred) &&
        {
            if (has_value_)
            {
                if constexpr (std::is_void_v<T>)
                {
                    if (std::invoke(pred))
                        return Option::Some();
                }
                else
                {
                    if (std::invoke(pred, value_))
                        return Option::Some(std::move(value_));
                }
            }
            return Option::None();
        }

    public:
        /**
         * @brief Moves the value out (if any), leaving `*this` as `None`.
         *
         * @return the previous state as a new `Option` (Some with the moved value, or None)
         */
        [[nodiscard]] Option take() noexcept
        {
            Option out = Option::None();
            if (has_value_)
            {
                ::new (static_cast<void *>(std::addressof(out.value_))) StoredT(std::move(value_));
                out.has_value_ = true;
                destroy_();
                has_value_ = false;
            }
            return out;
        }

        /**
         * @brief Sets the option to `Some(val)`, returning the previous state.
         *        Available only when `T` is non-void.
         *
         * @param val the new value
         * @return the previous `Option` (Some or None)
         */
        [[nodiscard]] Option replace(StoredT val)
            requires(!std::is_void_v<T>)
        {
            Option old = take();
            ::new (static_cast<void *>(std::addressof(value_))) StoredT(std::move(val));
            has_value_ = true;
            return old;
        }

        /**
         * @brief Sets the option to `Some(val)` (dropping any previous value) and returns a
         *        reference to it. Available only when `T` is non-void.
         *
         * @param val the new value
         * @return reference to the stored value
         */
        StoredT &insert(StoredT val)
            requires(!std::is_void_v<T>)
        {
            destroy_();
            ::new (static_cast<void *>(std::addressof(value_))) StoredT(std::move(val));
            has_value_ = true;
            return value_;
        }

        /**
         * @brief Returns a reference to the value, inserting @p val first if currently None.
         *        Available only when `T` is non-void.
         *
         * @param val the value inserted when None
         * @return reference to the stored value
         */
        StoredT &get_or_insert(StoredT val)
            requires(!std::is_void_v<T>)
        {
            if (!has_value_)
            {
                ::new (static_cast<void *>(std::addressof(value_))) StoredT(std::move(val));
                has_value_ = true;
            }
            return value_;
        }

    public:
        /**
         * @brief Converts to `Result<T, E>`: Some becomes `Ok`, None becomes `Err(e)`.
         *
         * @tparam E the error type
         * @param err the error value used when None
         * @return `Result<T, E>`
         */
        template <typename E>
            requires std::constructible_from<E, const E &>
        [[nodiscard]] Result<T, E> ok_or(E err) const
        {
            if (has_value_)
            {
                if constexpr (std::is_void_v<T>)
                    return Result<void, E>::Ok();
                else
                    return Result<T, E>::Ok(value_);
            }
            return Result<T, E>::Err(std::move(err));
        }

        /**
         * @brief Converts to `Result<T, E>`: Some becomes `Ok`, None invokes `f()` to produce `Err`.
         *
         * @tparam F nullary callable returning `E`
         * @param f the error producer invoked when None
         * @return `Result<T, E>`
         */
        template <typename F>
            requires std::invocable<F>
        [[nodiscard]] auto ok_or_else(F &&f) const -> Result<T, std::invoke_result_t<F>>
        {
            using E = std::invoke_result_t<F>;
            if (has_value_)
            {
                if constexpr (std::is_void_v<T>)
                    return Result<void, E>::Ok();
                else
                    return Result<T, E>::Ok(value_);
            }
            return Result<T, E>::Err(std::invoke(f));
        }
    };
}

#endif // INCLUDE_PJH_RESULT_OPTION_HPP
