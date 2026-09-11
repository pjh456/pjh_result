/**
 * @file option.hpp
 * @brief Rust-style `Option<T>` backed by hand-written tagged storage.
 */
#ifndef INCLUDE_PJH_RESULT_OPTION_HPP
#define INCLUDE_PJH_RESULT_OPTION_HPP

#include <concepts>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

#include "pjh_result/detail/iterator.hpp"
#include "pjh_result/result.hpp"

namespace pjh::result
{
    template <typename T>
        requires detail::Storable<T>
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

        /// @brief Trait specialization exposing the value type of an `Option`, used by
        ///        the `OptionType` concept (mirrors `result_traits`).
        template <typename T>
        struct option_traits<Option<T>>
        {
            using value_type = T;
        };

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
     * @tparam T contained value type (may be `void`, but not a reference)
     *
     * @pre The move constructor of `T` (when non-void) must be `noexcept` (enforced by
     * the in-class `static_assert`), so assignment can destroy-then-nothrow-move and
     * never leave the object in an invalid state.
     * @note The return value must not be ignored (`[[nodiscard]]`).
     */
    template <typename T>
        requires detail::Storable<T>
    class [[nodiscard]] Option
    {
    private:
        /// @brief Grants every `Option<U>` specialization access to the state of other
        ///        instantiations, so cross-value-type `zip` / `zip_with` can read both
        ///        operands.
        template <typename U>
            requires detail::Storable<U>
        friend class Option;

        /// @brief Actual storage type; degrades to `Unit` when `T = void`.
        using StoredT = std::conditional_t<std::is_void_v<T>, Unit, T>;

        static_assert(
            std::is_nothrow_move_constructible_v<StoredT>,
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
        explicit Option(some_t, A &&...a) noexcept(
            std::is_nothrow_constructible_v<StoredT, A &&...>) :
            has_value_(true), value_(std::forward<A>(a)...)
        {
        }

        /// @brief Constructs the empty (None) state.
        explicit Option(none_t) noexcept : has_value_(false) {}

        /// @brief Destroys the contained value if present; a no-op for trivially
        /// destructible types.
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
                ::new (static_cast<void *>(std::addressof(value_)))
                    StoredT(std::move(o.value_));
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
        static Option Some(U &&val) noexcept(
            std::is_nothrow_constructible_v<StoredT, U &&>)
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
        static Option None() noexcept { return Option(none_t{}); }

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
                ::new (static_cast<void *>(std::addressof(value_)))
                    StoredT(std::move(o.value_));
        }

        /**
         * @brief Copy assignment (strong exception guarantee).
         *
         * First copy-constructs a temporary (if this step throws, `*this` is left
         * unchanged), then destroys the old value and nothrow move-constructs from the
         * temporary.
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
        /// @brief The value type (useful for metaprogramming).
        using value_type = T;

        /// @brief Immutable iterator over the contained value (`Some` yields one
        ///        element, `None` yields none).
        using Iter = detail::SingleIter<StoredT, true>;
        /// @brief Mutable iterator over the contained value (`Some` yields one
        ///        element, `None` yields none).
        using IterMut = detail::SingleIter<StoredT, false>;

        /**
         * @brief Returns an iterator over the contained value.
         *
         * Yields exactly one element (`const T &`) when `is_some()`, and an empty
         * range when `is_none()`. The iterator is itself a range, so
         * `for (const auto &x : o.iter())` and `std::ranges::find(o.iter(), v)`
         * both work. Available only when `T` is non-void.
         *
         * @return a zero-or-one element forward iterator borrowing `*this`
         * @warning The returned iterator borrows `*this`; it must not outlive the
         *          source and the source must not be reassigned, `take()`n from, or
         *          otherwise mutated while the iterator is in use.
         */
        [[nodiscard]] auto iter() const & noexcept -> Iter
            requires(!std::is_void_v<T>)
        {
            return has_value_ ? Iter{std::addressof(value_)} : Iter{};
        }

        /// @brief Deleted: an iterator into a temporary would dangle.
        void iter() && = delete;
        /// @brief Deleted: an iterator into a temporary would dangle.
        void iter() const && = delete;

        /**
         * @brief Returns a mutable iterator over the contained value.
         *
         * Yields exactly one element (`T &`) when `is_some()`, and an empty range
         * when `is_none()`. Writes through the iterator are visible on `*this`.
         * Callable only on a non-const lvalue; available only when `T` is non-void.
         *
         * @return a zero-or-one element forward iterator borrowing `*this`
         * @warning The returned iterator borrows `*this`; it must not outlive the
         *          source and the source must not be reassigned, `take()`n from, or
         *          otherwise mutated while the iterator is in use.
         */
        [[nodiscard]] auto iter_mut() & noexcept -> IterMut
            requires(!std::is_void_v<T>)
        {
            return has_value_ ? IterMut{std::addressof(value_)} : IterMut{};
        }

        /**
         * @brief Returns a mutable iterator over the contained value (range-for entry).
         *
         * Allows `for (auto &x : o)` and classic algorithms. Available only when
         * `T` is non-void.
         *
         * @return a mutable zero-or-one element forward iterator
         * @warning An iterator stored out of a temporary returned by this function
         *          dangles once the temporary dies (same rule as standard
         *          containers); use it only within a range-for or algorithm call.
         */
        [[nodiscard]] auto begin() noexcept -> IterMut
            requires(!std::is_void_v<T>)
        {
            return has_value_ ? IterMut{std::addressof(value_)} : IterMut{};
        }

        /// @brief One-past-the-end position of the value range.
        [[nodiscard]] auto end() noexcept -> IterMut
            requires(!std::is_void_v<T>)
        {
            return IterMut{};
        }

        /**
         * @brief Returns an immutable iterator over the contained value (range-for entry).
         *
         * Allows `for (const auto &x : std::as_const(o))` and classic algorithms on
         * a const option. Available only when `T` is non-void.
         *
         * @return an immutable zero-or-one element forward iterator
         */
        [[nodiscard]] auto begin() const noexcept -> Iter
            requires(!std::is_void_v<T>)
        {
            return has_value_ ? Iter{std::addressof(value_)} : Iter{};
        }

        /// @brief One-past-the-end position of the const value range.
        [[nodiscard]] auto end() const noexcept -> Iter
            requires(!std::is_void_v<T>)
        {
            return Iter{};
        }

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

        /**
         * @brief Whether the option is None and the nullary predicate @p f returns true.
         *
         * @tparam F nullary predicate
         * @param f the predicate
         * @return `is_none() && bool(f())`
         */
        template <typename F>
            requires std::invocable<F>
        [[nodiscard]] bool is_none_and(F &&f) const
        {
            return !has_value_ && static_cast<bool>(std::invoke(f));
        }

        /**
         * @brief Whether the option is Some and the value equals @p val.
         *
         * @param val the value to compare against
         * @return `is_some() && value == val`
         */
        [[nodiscard]] bool contains(const StoredT &val) const
            requires(!std::is_void_v<T>) && std::equality_comparable<StoredT>
        {
            return has_value_ && value_ == val;
        }

        /**
         * @brief Returns a borrowing view of the contained value (`Some`) or `None`.
         *
         * The view holds a `std::reference_wrapper` pointing into `*this`; it neither
         * moves nor copies the value and leaves the source unchanged. Available only
         * when `T` is non-void.
         *
         * @return `Option<std::reference_wrapper<const T>>`
         * @warning The returned view borrows `*this`. It must not outlive the source
         *          and must not be used after the source is destroyed or reassigned.
         */
        template <typename U = T>
            requires(!std::is_void_v<U>)
        [[nodiscard]] auto as_ref() const &
            -> Option<std::reference_wrapper<const U>>
        {
            using R = Option<std::reference_wrapper<const U>>;
            return has_value_ ? R::Some(std::cref(value_)) : R::None();
        }

        /// @brief Deleted: a view into a temporary would dangle.
        template <typename U = T>
            requires(!std::is_void_v<U>)
        auto as_ref() && = delete;

        /// @brief Deleted: a view into a temporary would dangle.
        template <typename U = T>
            requires(!std::is_void_v<U>)
        auto as_ref() const && = delete;

        /**
         * @brief Returns a mutable borrowing view of the contained value or `None`.
         *
         * Writes made through the returned view are visible on `*this`. Callable only
         * on a non-const lvalue; available only when `T` is non-void.
         *
         * @return `Option<std::reference_wrapper<T>>`
         * @warning The returned view borrows `*this`. It must not outlive the source
         *          and must not be used after the source is destroyed or reassigned.
         */
        template <typename U = T>
            requires(!std::is_void_v<U>)
        [[nodiscard]] auto as_mut() &
            -> Option<std::reference_wrapper<U>>
        {
            using R = Option<std::reference_wrapper<U>>;
            return has_value_ ? R::Some(std::ref(value_)) : R::None();
        }

        /**
         * @brief Transposes an `Option<Result<U, E>>` into `Result<Option<U>, E>`.
         *
         * `Some(Ok(u))` becomes `Ok(Some(u))`, `Some(Err(e))` becomes `Err(e)`,
         * `None` becomes `Ok(None)`.
         *
         * @tparam V Result type (inferred from T being a Result)
         * @return the transposed `Result`
         */
        template <typename V = T>
            requires detail::ResultType<V>
        [[nodiscard]] auto transpose() const
            & -> Result<Option<detail::result_value_t<V>>, detail::result_error_t<V>>
        {
            using InnerV = detail::result_value_t<V>;
            using InnerE = detail::result_error_t<V>;
            using Out = Result<Option<InnerV>, InnerE>;

            if (!has_value_)
                return Out::Ok(Option<InnerV>::None());

            if (value_.is_ok())
            {
                if constexpr (std::is_void_v<InnerV>)
                {
                    value_.unwrap();
                    return Out::Ok(Option<void>::Some());
                }
                else
                {
                    return Out::Ok(Option<InnerV>::Some(value_.unwrap()));
                }
            }
            return Out::Err(value_.unwrap_err());
        }

        /// @overload
        template <typename V = T>
            requires detail::ResultType<V>
        [[nodiscard]] auto transpose()
            && -> Result<Option<detail::result_value_t<V>>, detail::result_error_t<V>>
        {
            using InnerV = detail::result_value_t<V>;
            using InnerE = detail::result_error_t<V>;
            using Out = Result<Option<InnerV>, InnerE>;

            if (!has_value_)
                return Out::Ok(Option<InnerV>::None());

            auto inner = std::move(value_);
            destroy_();
            has_value_ = false;

            if (inner.is_ok())
            {
                if constexpr (std::is_void_v<InnerV>)
                {
                    std::move(inner).unwrap();
                    return Out::Ok(Option<void>::Some());
                }
                else
                {
                    return Out::Ok(Option<InnerV>::Some(std::move(inner).unwrap()));
                }
            }
            return Out::Err(std::move(inner).unwrap_err());
        }

        /**
         * @brief Zips two Options into a single Option of a pair.
         *
         * `Some(a)` with `Some(b)` becomes `Some((a, b))`; otherwise `None`.
         *
         * @tparam U the other Option's value type
         * @param other the second Option
         * @return `Option<std::pair<T, U>>`
         */
        template <typename U>
            requires(!std::is_void_v<T>) && (!std::is_void_v<U>)
        [[nodiscard]] Option<std::pair<T, U>> zip(Option<U> other) const &
        {
            if (has_value_ && other.has_value_)
                return Option<std::pair<T, U>>::Some(
                    std::make_pair(value_, other.value_));
            return Option<std::pair<T, U>>::None();
        }

        /// @overload
        template <typename U>
            requires(!std::is_void_v<T>) && (!std::is_void_v<U>)
        [[nodiscard]] Option<std::pair<T, U>> zip(Option<U> other) &&
        {
            if (has_value_ && other.has_value_)
            {
                auto p = std::make_pair(std::move(value_), std::move(other.value_));
                destroy_();
                has_value_ = false;
                return Option<std::pair<T, U>>::Some(std::move(p));
            }
            if (has_value_)
            {
                destroy_();
                has_value_ = false;
            }
            return Option<std::pair<T, U>>::None();
        }

        /**
         * @brief Zips two Options with a combiner function.
         *
         * `Some(a)` with `Some(b)` becomes `Some(f(a, b))`; otherwise `None`.
         *
         * @tparam U the other Option's value type
         * @tparam F callable `(T, U) -> R`
         * @param other the second Option
         * @param f the combiner
         * @return `Option<R>`
         */
        template <typename U, typename F>
            requires(!std::is_void_v<T>) && (!std::is_void_v<U>) &&
                    std::invocable<F, const T &, U &> &&
                    (!std::is_void_v<std::invoke_result_t<F, const T &, U &>>)
        [[nodiscard]] auto zip_with(Option<U> other, F &&f)
            const & -> Option<decltype(std::invoke(f, value_, other.value_))>
        {
            using R = decltype(std::invoke(f, value_, other.value_));
            if (has_value_ && other.has_value_)
                return Option<R>::Some(std::invoke(f, value_, other.value_));
            return Option<R>::None();
        }

        /// @overload
        template <typename U, typename F>
            requires(!std::is_void_v<T>) && (!std::is_void_v<U>) &&
                    std::invocable<F, T &&, U &&> &&
                    (!std::is_void_v<std::invoke_result_t<F, T &&, U &&>>)
        [[nodiscard]] auto zip_with(Option<U> other, F &&f) && -> Option<
            decltype(std::invoke(f, std::move(value_), std::move(other.value_)))>
        {
            using R =
                decltype(std::invoke(f, std::move(value_), std::move(other.value_)));
            if (has_value_ && other.has_value_)
            {
                auto r = Option<R>::Some(
                    std::invoke(f, std::move(value_), std::move(other.value_)));
                destroy_();
                has_value_ = false;
                return r;
            }
            if (has_value_)
            {
                destroy_();
                has_value_ = false;
            }
            return Option<R>::None();
        }

        /**
         * @brief Splits `Some((a, b))` into `(Some(a), Some(b))`; `None` becomes
         *        `(None, None)`. Inverse of `zip`.
         *
         * Available only when the value type is a `std::pair<A, B>` with non-reference
         * elements; bare pair-like types and reference-element pairs are rejected.
         *
         * @tparam U `std::pair<A, B>` value type (deduced from `T`)
         * @return `std::pair<Option<A>, Option<B>>` for a value type `std::pair<A, B>`
         */
        template <typename U = T>
            requires detail::PairType<U>
        [[nodiscard]] auto unzip() const &
            -> std::pair<Option<typename U::first_type>, Option<typename U::second_type>>
        {
            using A = typename U::first_type;
            using B = typename U::second_type;
            if (has_value_)
                return std::make_pair(Option<A>::Some(value_.first),
                                      Option<B>::Some(value_.second));
            return std::make_pair(Option<A>::None(), Option<B>::None());
        }

        /// @overload (rvalue: moves the pair elements and leaves `*this` as `None`)
        template <typename U = T>
            requires detail::PairType<U>
        [[nodiscard]] auto unzip() &&
            -> std::pair<Option<typename U::first_type>, Option<typename U::second_type>>
        {
            using A = typename U::first_type;
            using B = typename U::second_type;
            if (has_value_)
            {
                auto p = std::make_pair(Option<A>::Some(std::move(value_.first)),
                                        Option<B>::Some(std::move(value_.second)));
                destroy_();
                has_value_ = false;
                return p;
            }
            return std::make_pair(Option<A>::None(), Option<B>::None());
        }

        /**
         * @brief Returns `Some` on exactly one of `*this` and @p other being `Some`;
         *        `None` when both are `Some` or both are `None`.
         *
         * @param other the other Option
         * @return `Option<T>`
         */
        [[nodiscard]] Option x_or(Option other) const &
            requires(!std::is_void_v<T>)
        {
            if (has_value_ != other.has_value_)
                return has_value_ ? Option::Some(value_) : Option::Some(other.value_);
            return Option::None();
        }

        /// @overload
        [[nodiscard]] Option x_or(Option other) &&
            requires(!std::is_void_v<T>)
        {
            if (has_value_ != other.has_value_)
            {
                if (has_value_)
                {
                    auto v = Option::Some(std::move(value_));
                    destroy_();
                    has_value_ = false;
                    return v;
                }
                auto v = Option::Some(std::move(other.value_));
                return v;
            }
            if (has_value_)
            {
                destroy_();
                has_value_ = false;
            }
            return Option::None();
        }

    public:
        /**
         * @brief Unwraps the contained value; throws if None. Available only when `T` is
         * non-void.
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
         * @brief Unwraps the contained value; throws if None. Available only when `T` is
         * non-void.
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

        /// @brief Deleted: a `const` rvalue would leave a dangling reference.
        [[nodiscard]] const StoredT &unwrap() const &&
            requires(!std::is_void_v<T>) = delete;

        /**
         * @brief Unwraps and moves out the contained value; throws if None. Available
         * only when `T` is non-void.
         *
         * @throws bad_result_access when currently None
         * @return the moved-out value
         */
        [[nodiscard]] StoredT unwrap() &&
            requires(!std::is_void_v<T>)
        {
            if (!has_value_)
                throw bad_result_access("Option::unwrap() called on None");
            StoredT tmp = std::move(value_);
            destroy_();
            has_value_ = false;
            return tmp;
        }

        /**
         * @brief Asserts the option is Some; throws if None. Available only when `T` is
         * `void`.
         *
         * @throws bad_result_access when currently None
         */
        void unwrap() const &
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

        /// @brief Deleted: a `const` rvalue would leave a dangling reference.
        [[nodiscard]] const StoredT &expect(const std::string &msg) const &&
            requires(!std::is_void_v<T>) = delete;

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
            StoredT tmp = std::move(value_);
            destroy_();
            has_value_ = false;
            return tmp;
        }

        /**
         * @brief Asserts the option is Some, throwing @p msg if None.
         *        Available only when `T` is `void`.
         *
         * @param msg message carried by the thrown exception
         * @throws bad_result_access when currently None
         */
        void expect(const std::string &msg) const &
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
            requires detail::MapCallable<F, T> &&
                     (!std::is_void_v<detail::map_result_t<F, T>>)
        [[nodiscard]] detail::map_result_t<F, T> map_or(
            detail::map_result_t<F, T> def, F &&f) const
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
                     std::convertible_to<
                         std::invoke_result_t<D>,
                         detail::map_result_t<F, T>>
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
         * @warning Returns a reference to `*this`; do not call on a temporary and keep
         * the result.
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

        /// @brief Deleted: a `const` rvalue would leave a dangling reference.
        template <typename F>
            requires detail::MapCallable<F, T>
        const Option &inspect(F &&f) const && = delete;

        /**
         * @brief Invokes @p f on the value if Some, then returns `*this` by value.
         *
         * The rvalue counterpart of the `const &` overload: the observer runs before
         * the object is moved out, so the result stays valid when chained from a
         * temporary. When `T = void`, `f()` is called.
         *
         * @tparam F callable observing the value (or nullary when `T = void`)
         * @param f the observer
         * @return `*this` moved into a new `Option`
         */
        template <typename F>
            requires detail::MutMapCallable<F, T>
        [[nodiscard]] Option inspect(F &&f) &&
        {
            if (has_value_)
            {
                if constexpr (std::is_void_v<T>)
                    std::invoke(f);
                else
                    std::invoke(f, value_);
            }
            return std::move(*this);
        }

    public:
        /**
         * @brief Chains an option-returning operation (FlatMap / AndThen).
         *
         * On Some, invokes `f` (`f()` when `T = void`, otherwise `f(value)`; `f` must
         * return an `Option`); on None, short-circuits and returns `None`.
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
         * @brief Chains an option-returning operation (FlatMap / AndThen), rvalue/move
         * overload.
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
            requires detail::OptionType<std::invoke_result_t<F>> &&
                     ((std::is_void_v<T> &&
                       std::is_void_v<detail::option_value_t<std::invoke_result_t<F>>>) ||
                      (!std::is_void_v<T> &&
                       std::constructible_from<
                           detail::option_value_t<std::invoke_result_t<F>>, const T &>))
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
            requires detail::OptionType<std::invoke_result_t<F>> &&
                     ((std::is_void_v<T> &&
                       std::is_void_v<detail::option_value_t<std::invoke_result_t<F>>>) ||
                      (!std::is_void_v<T> &&
                       std::constructible_from<
                           detail::option_value_t<std::invoke_result_t<F>>, T &&>))
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
         * @brief Returns @p other if `*this` is Some, otherwise `None`.
         *
         * The eager counterpart of `and_then` (Rust `Option::and`): no closure is
         * involved, so @p other is evaluated unconditionally. The value type becomes
         * `U` (that of @p other).
         *
         * @tparam U value type of @p other
         * @param other the option yielded when `*this` is Some
         * @return @p other if Some, otherwise `None`
         */
        template <typename U>
        [[nodiscard]] Option<U> and_with(Option<U> other) const &
        {
            if (has_value_)
                return other;
            return Option<U>::None();
        }

        /// @overload (rvalue: consumes `*this`, which becomes `None`)
        template <typename U>
        [[nodiscard]] Option<U> and_with(Option<U> other) &&
        {
            if (has_value_)
            {
                destroy_();
                has_value_ = false;
                return other;
            }
            return Option<U>::None();
        }

        /**
         * @brief Returns `*this` if Some, otherwise @p other.
         *
         * The eager counterpart of `or_else` (Rust `Option::or`): no closure is
         * involved, so @p other is evaluated unconditionally. Both sides share the
         * value type `T`.
         *
         * @param other the option yielded when `*this` is None
         * @return `*this` if Some, otherwise @p other
         */
        [[nodiscard]] Option or_with(Option other) const &
        {
            if (has_value_)
                return *this;
            return other;
        }

        /// @overload (rvalue: moves the value out and leaves `*this` as `None`)
        [[nodiscard]] Option or_with(Option other) &&
        {
            if (has_value_)
                return take();
            return other;
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
            requires detail::MapCallable<F, T> && std::move_constructible<StoredT>
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

        /**
         * @brief Flattens one level of nesting.
         *
         * On `Some(inner)`, returns `inner`; on `None`, returns `None`.
         * Available only when the value type is itself an `Option<U>`.
         *
         * @tparam U Option value type (inferred from T)
         * @return `Option<U>` with the inner value
         */
        template <typename U = T>
            requires detail::OptionType<U>
        [[nodiscard]] auto flatten() const & -> Option<typename U::value_type>
        {
            if (!has_value_)
                return Option<typename U::value_type>::None();
            return value_;
        }

        /// @overload
        template <typename U = T>
            requires detail::OptionType<U>
        [[nodiscard]] auto flatten() && -> Option<typename U::value_type>
        {
            if (!has_value_)
                return Option<typename U::value_type>::None();
            auto inner = std::move(value_);
            destroy_();
            has_value_ = false;
            return inner;
        }

    public:
        /**
         * @brief Moves the value out (if any), leaving `*this` as `None`.
         *
         * @return the previous state as a new `Option` (Some with the moved value, or
         * None)
         */
        [[nodiscard]] Option take() noexcept
        {
            Option out = Option::None();
            if (has_value_)
            {
                ::new (static_cast<void *>(std::addressof(out.value_)))
                    StoredT(std::move(value_));
                out.has_value_ = true;
                destroy_();
                has_value_ = false;
            }
            return out;
        }

        /**
         * @brief Takes the value out only if `pred(value)` is true (Rust `Option::take_if`).
         *
         * The predicate receives a mutable reference and may modify the value even when
         * it returns false; the value is removed (and `*this` becomes `None`) only when
         * the predicate returns true. Callable only on an lvalue (Rust `&mut self`).
         *
         * @tparam F predicate on the value
         * @param pred the predicate
         * @return `Some(value)` if taken, otherwise `None`
         */
        template <typename F>
            requires(!std::is_void_v<T>) && std::invocable<F, StoredT &> &&
                    std::convertible_to<std::invoke_result_t<F, StoredT &>, bool>
        [[nodiscard]] Option take_if(F &&pred) &
        {
            if (has_value_ && static_cast<bool>(std::invoke(pred, value_)))
                return take();
            return Option::None();
        }

        /**
         * @overload (T = void: nullary predicate)
         */
        template <typename F>
            requires std::is_void_v<T> && std::invocable<F> &&
                     std::convertible_to<std::invoke_result_t<F>, bool>
        [[nodiscard]] Option take_if(F &&pred) &
        {
            if (has_value_ && static_cast<bool>(std::invoke(pred)))
                return take();
            return Option::None();
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
         * @brief Sets the option to `Some(val)` (dropping any previous value) and returns
         * a reference to it. Available only when `T` is non-void.
         *
         * @param val the new value
         * @return reference to the stored value
         * @note Callable only on a non-const lvalue; an rvalue temporary cannot be
         *       stored into and its reference would dangle.
         */
        StoredT &insert(StoredT val) &
            requires(!std::is_void_v<T>)
        {
            destroy_();
            ::new (static_cast<void *>(std::addressof(value_))) StoredT(std::move(val));
            has_value_ = true;
            return value_;
        }

        /**
         * @brief Returns a reference to the value, inserting @p val first if currently
         * None. Available only when `T` is non-void.
         *
         * @param val the value inserted when None
         * @return reference to the stored value
         * @note Callable only on a non-const lvalue.
         */
        StoredT &get_or_insert(StoredT val) &
            requires(!std::is_void_v<T>)
        {
            if (!has_value_)
            {
                ::new (static_cast<void *>(std::addressof(value_)))
                    StoredT(std::move(val));
                has_value_ = true;
            }
            return value_;
        }

        /**
         * @brief Returns a reference to the value, default-constructing it first if
         * currently None. Available only when `T` is non-void and default-initializable.
         *
         * @return reference to the stored value
         * @note Callable only on a non-const lvalue.
         */
        StoredT &get_or_insert_default() &
            requires(!std::is_void_v<T>) && std::default_initializable<StoredT>
        {
            if (!has_value_)
            {
                ::new (static_cast<void *>(std::addressof(value_))) StoredT{};
                has_value_ = true;
            }
            return value_;
        }

        /**
         * @brief Returns a reference to the value, calling @p f to produce it if
         * currently None. Available only when `T` is non-void.
         *
         * @tparam F nullary callable returning a value convertible to `StoredT`
         * @param f the fallback producer
         * @return reference to the stored value
         * @note Callable only on a non-const lvalue.
         */
        template <typename F>
            requires(!std::is_void_v<T>) && std::invocable<F> &&
                    std::convertible_to<std::invoke_result_t<F>, StoredT>
        StoredT &get_or_insert_with(F &&f) &
        {
            if (!has_value_)
            {
                ::new (static_cast<void *>(std::addressof(value_)))
                    StoredT(static_cast<StoredT>(std::invoke(f)));
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
         * @note `E` must form a valid `Result<T, E>`: it may not be `void`, a reference,
         *       a `Result`, or the same type as `T` (such errors are rejected at overload
         *       resolution instead of hard-erroring inside `Result`).
         */
        template <typename E>
            requires std::move_constructible<E> &&
                     detail::ValidResultTypes<T, E> && detail::NotResult<E> &&
                     (!std::is_void_v<E>) && (!std::is_reference_v<E>)
        [[nodiscard]] Result<T, E> ok_or(E err) const &
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

        /// @overload (rvalue: moves the value out and leaves `*this` as `None`)
        template <typename E>
            requires std::move_constructible<E> &&
                     detail::ValidResultTypes<T, E> && detail::NotResult<E> &&
                     (!std::is_void_v<E>) && (!std::is_reference_v<E>)
        [[nodiscard]] Result<T, E> ok_or(E err) &&
        {
            if (has_value_)
            {
                if constexpr (std::is_void_v<T>)
                {
                    destroy_();
                    has_value_ = false;
                    return Result<void, E>::Ok();
                }
                else
                {
                    auto v = std::move(value_);
                    destroy_();
                    has_value_ = false;
                    return Result<T, E>::Ok(std::move(v));
                }
            }
            return Result<T, E>::Err(std::move(err));
        }

        /**
         * @brief Converts to `Result<T, E>`: Some becomes `Ok`, None invokes `f()` to
         * produce `Err`.
         *
         * @tparam F nullary callable returning a non-void `E`
         * @param f the error producer invoked when None
         * @return `Result<T, E>`
         * @note A callable whose result is `void` is rejected (it would form the illegal
         *       `Result<T, void>`); the produced error type must likewise not be a
         *       reference, a `Result`, or the same type as `T`.
         */
        template <typename F>
            requires std::invocable<F> &&
                     detail::ValidResultTypes<T, std::invoke_result_t<F>> &&
                     detail::NotResult<std::invoke_result_t<F>> &&
                     (!std::is_void_v<std::invoke_result_t<F>>) &&
                     (!std::is_reference_v<std::invoke_result_t<F>>)
        [[nodiscard]] auto ok_or_else(F &&f) const & -> Result<T, std::invoke_result_t<F>>
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

        /// @overload (rvalue: moves the value out and leaves `*this` as `None`)
        template <typename F>
            requires std::invocable<F> &&
                     detail::ValidResultTypes<T, std::invoke_result_t<F>> &&
                     detail::NotResult<std::invoke_result_t<F>> &&
                     (!std::is_void_v<std::invoke_result_t<F>>) &&
                     (!std::is_reference_v<std::invoke_result_t<F>>)
        [[nodiscard]] auto ok_or_else(F &&f) && -> Result<T, std::invoke_result_t<F>>
        {
            using E = std::invoke_result_t<F>;
            if (has_value_)
            {
                if constexpr (std::is_void_v<T>)
                {
                    destroy_();
                    has_value_ = false;
                    return Result<void, E>::Ok();
                }
                else
                {
                    auto v = std::move(value_);
                    destroy_();
                    has_value_ = false;
                    return Result<T, E>::Ok(std::move(v));
                }
            }
            return Result<T, E>::Err(std::invoke(f));
        }

    public:
        /**
         * @brief Equality comparison.
         *
         * Two options are equal iff both are None, or both are Some with equal values
         * (or both Some when `T = void`).
         *
         * Available only when the stored type is equality-comparable. `operator!=` is
         * synthesized by the compiler (C++20).
         *
         * @param a left operand
         * @param b right operand
         * @return whether @p a and @p b are equal
         */
        friend bool operator==(const Option &a, const Option &b)
            requires(std::is_void_v<T> || std::equality_comparable<StoredT>)
        {
            if (a.has_value_ != b.has_value_)
                return false;
            if (!a.has_value_)
                return true;
            if constexpr (std::is_void_v<T>)
                return true;
            else
                return a.value_ == b.value_;
        }
    };
}

#endif  // INCLUDE_PJH_RESULT_OPTION_HPP
