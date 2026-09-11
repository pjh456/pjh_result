/**
 * @file result.hpp
 * @brief Rust-style `Result<T, E>` monad backed by hand-written tagged-union storage.
 */
#ifndef INCLUDE_PJH_RESULT_RESULT_HPP
#define INCLUDE_PJH_RESULT_RESULT_HPP

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "pjh_result/context.hpp"
#include "pjh_result/detail/iterator.hpp"
#include "pjh_result/detail/traits.hpp"
#include "pjh_result/errors.hpp"

namespace pjh::result
{
    template <typename T>
    class Option;

    /// @brief Placeholder type for the success branch when `T = void`.
    struct Unit
    {
    };

    namespace detail
    {
        /// @brief Discriminator marking which alternative (Ok or Err) is currently active.
        enum class Tag : unsigned char
        {
            Ok,
            Err,
            Moved
        };

        /// @brief Result type of `map`: `f()` when `T = void`, otherwise `f(const T&)`.
        ///        Evaluated lazily to avoid forming a `void` argument. When `F` is not
        ///        invocable (`const T&` for non-void, nullary for void) the trait degrades
        ///        to `void` instead of hard-erroring, so the alias stays usable in return
        ///        types while the `MapCallable` constraint rejects `F`.
        template <typename F, typename T, bool = std::is_void_v<T>>
        struct map_result
        {
            using type = std::invoke_result_t<F, const T &>;
        };
        template <typename F, typename T>
            requires(!std::invocable<F, const T &>)
        struct map_result<F, T, false>
        {
            using type = void;
        };
        template <typename F, typename T>
        struct map_result<F, T, true>
        {
            using type = std::invoke_result_t<F>;
        };
        template <typename F, typename T>
            requires(!std::invocable<F>)
        struct map_result<F, T, true>
        {
            using type = void;
        };
        template <typename F, typename T>
        using map_result_t = typename map_result<F, T>::type;

        /// @brief Result type of `and_then(const &)`: `f()` when `T = void`, otherwise `f(const T&)`.
        ///        When `F` is not invocable (`const T&` for non-void, nullary for void) the
        ///        trait degrades to `void` instead of hard-erroring, so the alias stays usable
        ///        in return types while the `CrefResultFn` constraint rejects `F`.
        template <typename F, typename T, bool = std::is_void_v<T>>
        struct cref_result
        {
            using type = std::invoke_result_t<F, const T &>;
        };
        template <typename F, typename T>
            requires(!std::invocable<F, const T &>)
        struct cref_result<F, T, false>
        {
            using type = void;
        };
        template <typename F, typename T>
        struct cref_result<F, T, true>
        {
            using type = std::invoke_result_t<F>;
        };
        template <typename F, typename T>
            requires(!std::invocable<F>)
        struct cref_result<F, T, true>
        {
            using type = void;
        };
        template <typename F, typename T>
        using cref_result_t = typename cref_result<F, T>::type;

        /// @brief Result type of `and_then(&&)`: `f()` when `T = void`, otherwise `f(T)`.
        ///        When `F` is not invocable (`T` for non-void, nullary for void) the trait
        ///        degrades to `void` instead of hard-erroring, so the alias stays usable in
        ///        return types while the `ValueResultFn` constraint rejects `F`.
        template <typename F, typename T, bool = std::is_void_v<T>>
        struct value_result
        {
            using type = std::invoke_result_t<F, T>;
        };
        template <typename F, typename T>
            requires(!std::invocable<F, T>)
        struct value_result<F, T, false>
        {
            using type = void;
        };
        template <typename F, typename T>
        struct value_result<F, T, true>
        {
            using type = std::invoke_result_t<F>;
        };
        template <typename F, typename T>
            requires(!std::invocable<F>)
        struct value_result<F, T, true>
        {
            using type = void;
        };
        template <typename F, typename T>
        using value_result_t = typename value_result<F, T>::type;

        /// @brief Result type of `map_err`: `f(const E&)`. When `F` is not invocable
        ///        the trait degrades to `void` instead of hard-erroring, so the alias
        ///        stays usable in return types while the constraint rejects `F`.
        template <typename F, typename E, bool = std::invocable<F, const E &>>
        struct map_err_result
        {
            using type = std::invoke_result_t<F, const E &>;
        };
        template <typename F, typename E>
        struct map_err_result<F, E, false>
        {
            using type = void;
        };
        template <typename F, typename E>
        using map_err_result_t = typename map_err_result<F, E>::type;

        /// @brief Whether `f` is callable on the success value passed by the const
        ///        members (which always bind it as `const T&`): requires `f()` when
        ///        `T = void`, else `f(const T&)`.
        template <typename F, typename T>
        concept MapCallable = (std::is_void_v<T> && std::invocable<F>) ||
                              (!std::is_void_v<T> && std::invocable<F, const T &>);

        /// @brief Whether `f` applied to the const success value returns a `Result`.
        template <typename F, typename T>
        concept CrefResultFn =
            (std::is_void_v<T> && ResultType<std::invoke_result_t<F>>) ||
            (!std::is_void_v<T> && ResultType<std::invoke_result_t<F, const T &>>);

        /// @brief Whether `f` applied to the (moved) success value returns a `Result`.
        template <typename F, typename T>
        concept ValueResultFn =
            (std::is_void_v<T> && ResultType<std::invoke_result_t<F>>) ||
            (!std::is_void_v<T> && ResultType<std::invoke_result_t<F, T>>);
    }

    /**
     * @brief Type-erases `T` in the propagation macros so that a value implicitly
     *        converts into a `Result` in the Err state.
     *
     * @tparam E error value type
     */
    template <typename E>
    struct Failure
    {
        E error;
    };

    /// @brief Deduction guide: allows writing `Failure{err}` instead of `Failure<E>{err}`.
    template <typename E>
    Failure(E) -> Failure<E>;

    /**
     * @brief A result monad.
     *
     * A Rust-like `Result<T, E>` that forces the caller to handle errors. It holds
     * either a success value `T` or an error `E` — exactly one of the two.
     *
     * Storage is a hand-written tagged union rather than `std::variant`:
     * - there is no "third state" such as `valueless_by_exception`;
     *   `is_ok()`, `is_err()` and `is_moved()` are mutually exclusive; exactly one
     *   holds at any time, except during construction and destruction.
     * - access does not go through `std::get`'s runtime check — the active member is
     *   read directly.
     *
     * `T = void` is supported: the success branch carries no value, `Ok()` takes no
     * argument, `unwrap()` returns `void`, and `unwrap_or` is not provided.
     *
     * @tparam T success value type (may be `void`)
     * @tparam E error value type
     *
     * @pre The move constructor of `T` (when non-void) and of `E` must be `noexcept`
     *      (enforced by the in-class `static_assert`). This lets assignment be
     *      implemented as "destroy the old value, then nothrow move-construct the new
     *      one", guaranteeing the object never enters an invalid state.
     * @note The return value must not be ignored (`[[nodiscard]]`).
     * @note `T` and `E` must not be the same type.
     */
    template <typename T, typename E>
        requires detail::ValidResultTypes<T, E> &&
                 detail::NotResult<E>
    class [[nodiscard]] Result
    {
    private:
        /// @brief Actual storage type of the success branch; degrades to `Unit` when `T = void`.
        using OkT = std::conditional_t<std::is_void_v<T>, Unit, T>;

        static_assert(std::is_nothrow_move_constructible_v<OkT>,
                      "pjh::result::Result requires T to be nothrow move constructible");
        static_assert(std::is_nothrow_move_constructible_v<E>,
                      "pjh::result::Result requires E to be nothrow move constructible");

        detail::Tag tag_;
        union
        {
            OkT ok_;
            E err_;
        };

        struct ok_t
        {
        };
        struct err_t
        {
        };

        /// @brief In-place constructs the Ok branch (success value forwarded from `a...`,
        ///        value-initialized when no argument is given).
        template <typename... A>
            requires std::constructible_from<OkT, A &&...>
        explicit Result(ok_t, A &&...a) noexcept(std::is_nothrow_constructible_v<OkT, A &&...>)
            : tag_(detail::Tag::Ok), ok_(std::forward<A>(a)...)
        {
        }

        /// @brief In-place constructs the Err branch (error forwarded from `a...`).
        template <typename... A>
            requires std::constructible_from<E, A &&...>
        explicit Result(err_t, A &&...a) noexcept(std::is_nothrow_constructible_v<E, A &&...>)
            : tag_(detail::Tag::Err), err_(std::forward<A>(a)...)
        {
        }

        /// @brief Destroys the currently active member; a no-op for trivially destructible types.
        void destroy_() noexcept
        {
            if (tag_ == detail::Tag::Moved)
                return;
            if (tag_ == detail::Tag::Ok)
            {
                if constexpr (!std::is_trivially_destructible_v<OkT>)
                    ok_.~OkT();
            }
            else
            {
                if constexpr (!std::is_trivially_destructible_v<E>)
                    err_.~E();
            }
        }

        /// @brief Throws if the Result is in the Moved (post-unwrap) state.
        void require_not_moved_() const
        {
            if (tag_ == detail::Tag::Moved)
                throw bad_result_access("Result used after move");
        }

        /// @brief Nothrow move-constructs this object's active member from an rvalue `Result`
        ///        (assumes this object's storage is empty / already destroyed). A `Moved`
        ///        source inherits the `Moved` state without constructing any member, so the
        ///        inactive union member is never read.
        void construct_from_(Result &&o) noexcept
        {
            tag_ = o.tag_;
            if (tag_ == detail::Tag::Ok)
                ::new (static_cast<void *>(std::addressof(ok_))) OkT(std::move(o.ok_));
            else if (tag_ == detail::Tag::Err)
                ::new (static_cast<void *>(std::addressof(err_))) E(std::move(o.err_));
        }

    public:
        /**
         * @brief Constructs a success result `Ok(val)`. Available only when `T` is non-void.
         *
         * @tparam U argument type used to construct `T`
         * @param val the success value
         * @return a `Result` in the Ok state
         */
        template <typename U>
            requires(!std::is_void_v<T>) && std::constructible_from<OkT, U &&>
        static Result Ok(U &&val) noexcept(std::is_nothrow_constructible_v<OkT, U &&>)
        {
            return Result(ok_t{}, std::forward<U>(val));
        }

        /**
         * @brief Constructs a success result `Ok()`. Available only when `T` is `void`.
         *
         * @return a `Result` in the Ok state
         */
        static Result Ok() noexcept
            requires std::is_void_v<T>
        {
            return Result(ok_t{});
        }

        /**
         * @brief Constructs an error result `Err(err)`.
         *
         * @tparam G argument type used to construct `E`
         * @param err the error value
         * @return a `Result` in the Err state
         */
        template <typename G>
            requires std::constructible_from<E, G &&>
        static Result Err(G &&err) noexcept(std::is_nothrow_constructible_v<E, G &&>)
        {
            return Result(err_t{}, std::forward<G>(err));
        }

    public:
        /// @brief Copy constructor: copies the other object's active member.
        /// @throws bad_result_access when @p o is in the Moved state (a moved result is
        ///         not copyable); no member is constructed in that case.
        Result(const Result &o)
            requires std::copy_constructible<OkT> && std::copy_constructible<E>
            : tag_(o.tag_)
        {
            if (tag_ == detail::Tag::Moved)
                throw bad_result_access("Result copy from moved");
            if (tag_ == detail::Tag::Ok)
                ::new (static_cast<void *>(std::addressof(ok_))) OkT(o.ok_);
            else
                ::new (static_cast<void *>(std::addressof(err_))) E(o.err_);
        }

        /// @brief Move constructor: nothrow-moves the other object's active member.
        ///        A `Moved` source yields a `Moved` result without touching the
        ///        inactive union member.
        Result(Result &&o) noexcept : tag_(o.tag_)
        {
            if (tag_ == detail::Tag::Ok)
                ::new (static_cast<void *>(std::addressof(ok_))) OkT(std::move(o.ok_));
            else if (tag_ == detail::Tag::Err)
                ::new (static_cast<void *>(std::addressof(err_))) E(std::move(o.err_));
        }

        /**
         * @brief Copy assignment (strong exception guarantee).
         *
         * First copy-constructs a temporary (if this step throws, `*this` is left
         * unchanged), then destroys the old value and nothrow move-constructs from the
         * temporary. Hence the object never enters an invalid state.
         */
        Result &operator=(const Result &o)
            requires std::copy_constructible<OkT> && std::copy_constructible<E>
        {
            if (this != std::addressof(o))
            {
                Result tmp(o);
                destroy_();
                construct_from_(std::move(tmp));
            }
            return *this;
        }

        /// @brief Move assignment: destroys the old value, then nothrow move-constructs.
        Result &operator=(Result &&o) noexcept
        {
            if (this != std::addressof(o))
            {
                destroy_();
                construct_from_(std::move(o));
            }
            return *this;
        }

        /// @brief Destructor: destroys the currently active member.
        ~Result() { destroy_(); }

        /**
         * @brief Implicitly constructs an Err result from a `Failure` (copying the error).
         *
         * @tparam G error type carried by the `Failure`
         * @param f the error carrier
         */
        template <typename G>
            requires std::constructible_from<E, const G &>
        Result(const Failure<G> &f) noexcept(std::is_nothrow_constructible_v<E, const G &>)
            : tag_(detail::Tag::Err), err_(f.error)
        {
        }

        /**
         * @brief Implicitly constructs an Err result from a `Failure` (moving the error).
         *
         * @tparam G error type carried by the `Failure`
         * @param f the error carrier
         */
        template <typename G>
            requires std::constructible_from<E, G &&>
        Result(Failure<G> &&f) noexcept(std::is_nothrow_constructible_v<E, G &&>)
            : tag_(detail::Tag::Err), err_(std::forward<G>(f.error))
        {
        }

    public:
        /// @brief Whether the result is currently in the Ok state.
        bool is_ok() const noexcept { return tag_ == detail::Tag::Ok; }
        /// @brief Whether the result is currently in the Err state.
        bool is_err() const noexcept { return tag_ == detail::Tag::Err; }
        /// @brief Whether the result is in the moved-from state (post rvalue-unwrap).
        bool is_moved() const noexcept { return tag_ == detail::Tag::Moved; }

        /// @brief Immutable iterator over the success value (`Ok` yields one element,
        ///        `Err` yields none).
        using Iter = detail::SingleIter<OkT, true>;
        /// @brief Mutable iterator over the success value (`Ok` yields one element,
        ///        `Err` yields none).
        using IterMut = detail::SingleIter<OkT, false>;

        /**
         * @brief Returns an iterator over the success value.
         *
         * Yields exactly one element (`const T &`) when `is_ok()`, and an empty
         * range when `is_err()`. The iterator is itself a range, so
         * `for (const auto &x : r.iter())` and `std::ranges::find(r.iter(), v)`
         * both work. Available only when `T` is non-void.
         *
         * @return a zero-or-one element forward iterator borrowing `*this`
         * @throws bad_result_access when the result is in the Moved state
         * @warning The returned iterator borrows `*this`; it must not outlive the
         *          source and the source must not be reassigned or consumed while
         *          the iterator is in use.
         */
        [[nodiscard]] auto iter() const & -> Iter
            requires(!std::is_void_v<T>)
        {
            require_not_moved_();
            return is_ok() ? Iter{std::addressof(ok_)} : Iter{};
        }

        /// @brief Deleted: an iterator into a temporary would dangle.
        void iter() && = delete;
        /// @brief Deleted: an iterator into a temporary would dangle.
        void iter() const && = delete;

        /**
         * @brief Returns a mutable iterator over the success value.
         *
         * Yields exactly one element (`T &`) when `is_ok()`, and an empty range
         * when `is_err()`. Writes through the iterator are visible on `*this`.
         * Callable only on a non-const lvalue; available only when `T` is non-void.
         *
         * @return a zero-or-one element forward iterator borrowing `*this`
         * @throws bad_result_access when the result is in the Moved state
         * @warning The returned iterator borrows `*this`; it must not outlive the
         *          source and the source must not be reassigned or consumed while
         *          the iterator is in use.
         */
        [[nodiscard]] auto iter_mut() & -> IterMut
            requires(!std::is_void_v<T>)
        {
            require_not_moved_();
            return is_ok() ? IterMut{std::addressof(ok_)} : IterMut{};
        }

        /**
         * @brief Returns a mutable iterator over the success value (range-for entry).
         *
         * Allows `for (auto &x : r)` and classic algorithms. Available only when
         * `T` is non-void.
         *
         * @return a mutable zero-or-one element forward iterator
         * @throws bad_result_access when the result is in the Moved state
         * @warning An iterator stored out of a temporary returned by this function
         *          dangles once the temporary dies (same rule as standard
         *          containers); use it only within a range-for or algorithm call.
         */
        [[nodiscard]] auto begin() -> IterMut
            requires(!std::is_void_v<T>)
        {
            require_not_moved_();
            return is_ok() ? IterMut{std::addressof(ok_)} : IterMut{};
        }

        /// @brief One-past-the-end position of the success range.
        [[nodiscard]] auto end() noexcept -> IterMut
            requires(!std::is_void_v<T>)
        {
            return IterMut{};
        }

        /**
         * @brief Returns an immutable iterator over the success value (range-for entry).
         *
         * Allows `for (const auto &x : std::as_const(r))` and classic algorithms on
         * a const result. Available only when `T` is non-void.
         *
         * @return an immutable zero-or-one element forward iterator
         * @throws bad_result_access when the result is in the Moved state
         */
        [[nodiscard]] auto begin() const -> Iter
            requires(!std::is_void_v<T>)
        {
            require_not_moved_();
            return is_ok() ? Iter{std::addressof(ok_)} : Iter{};
        }

        /// @brief One-past-the-end position of the const success range.
        [[nodiscard]] auto end() const noexcept -> Iter
            requires(!std::is_void_v<T>)
        {
            return Iter{};
        }

        /**
         * @brief Whether the result is Ok and the success value satisfies @p f.
         *
         * When `T = void`, @p f is invoked with no argument.
         *
         * @tparam F predicate on the success value (or nullary when `T = void`)
         * @param f the predicate
         * @return `is_ok() && bool(f(...))`
         * @throws bad_result_access when the result is in the Moved state
         */
        template <typename F>
            requires detail::MapCallable<F, T>
        [[nodiscard]] bool is_ok_and(F &&f) const
        {
            require_not_moved_();
            if (!is_ok())
                return false;
            if constexpr (std::is_void_v<T>)
                return static_cast<bool>(std::invoke(f));
            else
                return static_cast<bool>(std::invoke(f, ok_));
        }

        /**
         * @brief Whether the result is Err and the error value satisfies @p f.
         *
         * @tparam F predicate on the error value (bound as `const E&`)
         * @param f the predicate
         * @return `is_err() && bool(f(error))`
         * @throws bad_result_access when the result is in the Moved state
         */
        template <typename F>
            requires std::invocable<F, const E &>
        [[nodiscard]] bool is_err_and(F &&f) const
        {
            require_not_moved_();
            return is_err() && static_cast<bool>(std::invoke(f, err_));
        }

        /**
         * @brief Whether the result is Ok and the success value equals @p val.
         *
         * @param val the value to compare against
         * @return `is_ok() && ok_ == val`
         * @throws bad_result_access when the result is in the Moved state
         */
        [[nodiscard]] bool contains(const OkT &val) const
            requires(!std::is_void_v<T>) && std::equality_comparable<OkT>
        {
            require_not_moved_();
            return is_ok() && ok_ == val;
        }

        /**
         * @brief Whether the result is Err and the error value equals @p val.
         *
         * @param val the error to compare against
         * @return `is_err() && err_ == val`
         * @throws bad_result_access when the result is in the Moved state
         */
        [[nodiscard]] bool contains_err(const E &val) const
            requires std::equality_comparable<E>
        {
            require_not_moved_();
            return is_err() && err_ == val;
        }

        /**
         * @brief Returns a borrowing view of the active branch (Ok value or Err error).
         *
         * The view holds a `std::reference_wrapper` into `*this`, so the value or error
         * is neither copied nor moved and the source is left unchanged. Available only
         * when `T` is non-void.
         *
         * @return `Result<std::reference_wrapper<const T>, std::reference_wrapper<const E>>`
         * @throws bad_result_access when the result is in the Moved state
         * @warning The returned view borrows `*this`. It must not outlive the source
         *          and must not be used after the source is destroyed or reassigned.
         */
        template <typename U = T>
            requires(!std::is_void_v<U>)
        [[nodiscard]] auto as_ref() const &
            -> Result<std::reference_wrapper<const U>, std::reference_wrapper<const E>>
        {
            using R =
                Result<std::reference_wrapper<const U>, std::reference_wrapper<const E>>;
            require_not_moved_();
            return is_ok() ? R::Ok(std::cref(ok_)) : R::Err(std::cref(err_));
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
         * @brief Returns a mutable borrowing view of the active branch.
         *
         * Writes made through the returned view are visible on `*this`. Callable only
         * on a non-const lvalue; available only when `T` is non-void.
         *
         * @return `Result<std::reference_wrapper<T>, std::reference_wrapper<E>>`
         * @throws bad_result_access when the result is in the Moved state
         * @warning The returned view borrows `*this`. It must not outlive the source
         *          and must not be used after the source is destroyed or reassigned.
         */
        template <typename U = T>
            requires(!std::is_void_v<U>)
        [[nodiscard]] auto as_mut() &
            -> Result<std::reference_wrapper<U>, std::reference_wrapper<E>>
        {
            using R = Result<std::reference_wrapper<U>, std::reference_wrapper<E>>;
            require_not_moved_();
            return is_ok() ? R::Ok(std::ref(ok_)) : R::Err(std::ref(err_));
        }

        /**
         * @brief Returns a borrowing view of the error branch when `T = void`.
         *
         * With no success value to borrow, only the error is wrapped.
         *
         * @return `Result<void, std::reference_wrapper<const E>>`
         * @throws bad_result_access when the result is in the Moved state
         * @warning The returned view borrows `*this`. It must not outlive the source
         *          and must not be used after the source is destroyed or reassigned.
         */
        [[nodiscard]] auto as_ref() const &
            -> Result<void, std::reference_wrapper<const E>>
            requires std::is_void_v<T>
        {
            using R = Result<void, std::reference_wrapper<const E>>;
            require_not_moved_();
            return is_ok() ? R::Ok() : R::Err(std::cref(err_));
        }

        /// @brief Deleted: a view into a temporary would dangle.
        auto as_ref() && = delete;

        /// @brief Deleted: a view into a temporary would dangle.
        auto as_ref() const && = delete;

        /**
         * @brief Returns a mutable borrowing view of the error branch when `T = void`.
         *
         * Writes made through the returned view are visible on `*this`. Callable only
         * on a non-const lvalue.
         *
         * @return `Result<void, std::reference_wrapper<E>>`
         * @throws bad_result_access when the result is in the Moved state
         * @warning The returned view borrows `*this`. It must not outlive the source
         *          and must not be used after the source is destroyed or reassigned.
         */
        [[nodiscard]] auto as_mut() &
            -> Result<void, std::reference_wrapper<E>>
            requires std::is_void_v<T>
        {
            using R = Result<void, std::reference_wrapper<E>>;
            require_not_moved_();
            return is_ok() ? R::Ok() : R::Err(std::ref(err_));
        }

    public:
        /**
         * @brief Unwraps the success value; throws if Err. Available only when `T` is non-void.
         *
         * @throws bad_result_access when currently in the Err state
         * @return reference to the inner success value
         */
        [[nodiscard]] OkT &unwrap() &
            requires(!std::is_void_v<T>)
        {
            require_not_moved_();
            if (!is_ok())
                throw bad_result_access("Result::unwrap() called on Err");
            return ok_;
        }

        /**
         * @brief Unwraps the success value; throws if Err. Available only when `T` is non-void.
         *
         * @throws bad_result_access when currently in the Err state
         * @return const reference to the inner success value
         */
        [[nodiscard]] const OkT &unwrap() const &
            requires(!std::is_void_v<T>)
        {
            require_not_moved_();
            if (!is_ok())
                throw bad_result_access("Result::unwrap() called on Err");
            return ok_;
        }

        /**
         * @brief Unwraps and moves out the success value; throws if Err. Available only when
         *        `T` is non-void.
         *
         * @throws bad_result_access when currently in the Err state
         * @return the moved-out success value
         */
        [[nodiscard]] OkT unwrap() &&
            requires(!std::is_void_v<T>)
        {
            if (!is_ok())
                throw bad_result_access("Result::unwrap() called on Err");
            OkT tmp = std::move(ok_);
            destroy_();
            tag_ = detail::Tag::Moved;
            return tmp;
        }

        /**
         * @brief Asserts the state is Ok; throws if Err. Available only when `T` is `void`.
         *
         * @throws bad_result_access when currently in the Err state
         */
        void unwrap() const &
            requires std::is_void_v<T>
        {
            require_not_moved_();
            if (is_err())
                throw bad_result_access("Result<void>::unwrap() called on Err");
        }

        /**
         * @brief Unwraps the success value, or returns the given default if Err.
         *        Available only when `T` is non-void.
         *
         * @param val the default returned when in the Err state
         * @return the success value, or @p val
         * @throws bad_result_access when the result is in the Moved state
         */
        [[nodiscard]] OkT unwrap_or(OkT val) const
            requires(!std::is_void_v<T>)
        {
            require_not_moved_();
            return is_ok() ? ok_ : std::move(val);
        }

        /**
         * @brief Unwraps the success value, throwing with a custom message if Err.
         *        Available only when `T` is non-void.
         *
         * @param msg message carried by the thrown exception
         * @throws bad_result_access when currently in the Err state
         * @return reference to the inner success value
         */
        [[nodiscard]] OkT &expect(const std::string &msg) &
            requires(!std::is_void_v<T>)
        {
            require_not_moved_();
            if (!is_ok())
                throw bad_result_access(msg);
            return ok_;
        }

        /**
         * @brief Unwraps the success value (const overload), throwing @p msg if Err.
         *        Available only when `T` is non-void.
         *
         * @param msg message carried by the thrown exception
         * @throws bad_result_access when currently in the Err state
         * @return const reference to the inner success value
         */
        [[nodiscard]] const OkT &expect(const std::string &msg) const &
            requires(!std::is_void_v<T>)
        {
            require_not_moved_();
            if (!is_ok())
                throw bad_result_access(msg);
            return ok_;
        }

        /**
         * @brief Unwraps and moves out the success value, throwing @p msg if Err.
         *        Available only when `T` is non-void.
         *
         * @param msg message carried by the thrown exception
         * @throws bad_result_access when currently in the Err state
         * @return the moved-out success value
         */
        [[nodiscard]] OkT expect(const std::string &msg) &&
            requires(!std::is_void_v<T>)
        {
            if (!is_ok())
                throw bad_result_access(msg);
            OkT tmp = std::move(ok_);
            destroy_();
            tag_ = detail::Tag::Moved;
            return tmp;
        }

        /**
         * @brief Asserts the state is Ok, throwing @p msg if Err.
         *        Available only when `T` is `void`.
         *
         * @param msg message carried by the thrown exception
         * @throws bad_result_access when currently in the Err state
         */
        void expect(const std::string &msg) const &
            requires std::is_void_v<T>
        {
            require_not_moved_();
            if (is_err())
                throw bad_result_access(msg);
        }

        /**
         * @brief Unwraps the success value, or computes a fallback from the error if Err.
         *        Available only when `T` is non-void.
         *
         * @tparam F callable taking `E` and returning a value convertible to `T`
         * @param f fallback producer invoked in the Err state
         * @return the success value, or `f(error)`
         */
        template <typename F>
            requires(!std::is_void_v<T>) && std::invocable<F, const E &> &&
                    std::convertible_to<std::invoke_result_t<F, const E &>, T>
        [[nodiscard]] T unwrap_or_else(F &&f) const
        {
            require_not_moved_();
            if (is_ok())
                return ok_;
            return static_cast<T>(std::invoke(f, err_));
        }

        /**
         * @brief Unwraps the success value, or returns a default-constructed `T` if Err.
         *        Available only when `T` is non-void and default-initializable.
         *
         * @return the success value, or `T{}`
         * @throws bad_result_access when the result is in the Moved state
         */
        [[nodiscard]] T unwrap_or_default() const
            requires(!std::is_void_v<T>) && std::default_initializable<T>
        {
            require_not_moved_();
            if (is_ok())
                return ok_;
            return T{};
        }

        /**
         * @brief Unwraps the error value; throws if Ok.
         *
         * @throws bad_result_access when currently in the Ok state
         * @return reference to the inner error value
         */
        [[nodiscard]] E &unwrap_err() &
        {
            require_not_moved_();
            if (!is_err())
                throw bad_result_access("Result::unwrap_err() called on Ok");
            return err_;
        }

        /**
         * @brief Unwraps the error value; throws if Ok.
         *
         * @throws bad_result_access when currently in the Ok state
         * @return const reference to the inner error value
         */
        [[nodiscard]] const E &unwrap_err() const &
        {
            require_not_moved_();
            if (!is_err())
                throw bad_result_access("Result::unwrap_err() called on Ok");
            return err_;
        }

        /**
         * @brief Unwraps and moves out the error value; throws if Ok.
         *
         * @throws bad_result_access when currently in the Ok state
         * @return the moved-out error value
         */
        [[nodiscard]] E unwrap_err() &&
        {
            if (!is_err())
                throw bad_result_access("Result::unwrap_err() called on Ok");
            E tmp = std::move(err_);
            destroy_();
            tag_ = detail::Tag::Moved;
            return tmp;
        }

        /**
         * @brief Unwraps the error value, or returns the given default if Ok.
         *
         * @param err the default returned when in the Ok state
         * @return the error value, or @p err
         * @throws bad_result_access when the result is in the Moved state
         */
        [[nodiscard]] E unwrap_err_or(E err) const
        {
            require_not_moved_();
            return is_err() ? err_ : std::move(err);
        }

        /**
         * @brief Unwraps the error value, or computes a fallback from the success value
         *        if Ok. Lazy counterpart of `unwrap_err_or` (not a Rust std API).
         *        Available only when `T` is non-void.
         *
         * @tparam F callable taking the success value and returning a value convertible
         *         to `E`
         * @param f fallback producer invoked in the Ok state
         * @return the error value, or `f(success)`
         */
        template <typename F>
            requires(!std::is_void_v<T>) && std::invocable<F, const T &> &&
                    std::convertible_to<std::invoke_result_t<F, const T &>, E>
        [[nodiscard]] E unwrap_err_or_else(F &&f) const
        {
            require_not_moved_();
            if (is_err())
                return err_;
            return static_cast<E>(std::invoke(f, ok_));
        }

        /**
         * @overload (T = void: nullary fallback producer)
         */
        template <typename F>
            requires std::is_void_v<T> && std::invocable<F> &&
                     std::convertible_to<std::invoke_result_t<F>, E>
        [[nodiscard]] E unwrap_err_or_else(F &&f) const
        {
            require_not_moved_();
            if (is_err())
                return err_;
            return static_cast<E>(std::invoke(f));
        }

        /**
         * @brief Unwraps the error value, throwing with a custom message if Ok.
         *
         * @param msg message carried by the thrown exception
         * @throws bad_result_access when currently in the Ok state
         * @return reference to the inner error value
         */
        [[nodiscard]] E &expect_err(const std::string &msg) &
        {
            require_not_moved_();
            if (!is_err())
                throw bad_result_access(msg);
            return err_;
        }

        /**
         * @brief Unwraps the error value (const overload), throwing @p msg if Ok.
         *
         * @param msg message carried by the thrown exception
         * @throws bad_result_access when currently in the Ok state
         * @return const reference to the inner error value
         */
        [[nodiscard]] const E &expect_err(const std::string &msg) const &
        {
            require_not_moved_();
            if (!is_err())
                throw bad_result_access(msg);
            return err_;
        }

        /**
         * @brief Unwraps and moves out the error value, throwing @p msg if Ok.
         *
         * @param msg message carried by the thrown exception
         * @throws bad_result_access when currently in the Ok state
         * @return the moved-out error value
         */
        [[nodiscard]] E expect_err(const std::string &msg) &&
        {
            if (!is_err())
                throw bad_result_access(msg);
            E tmp = std::move(err_);
            destroy_();
            tag_ = detail::Tag::Moved;
            return tmp;
        }

    public:
        /**
         * @brief Transforms the success value (Map).
         *
         * On Ok, applies `f` to the success value (calls `f()` when `T = void`) and returns
         * `Ok(f(...))`; on Err(e), returns `Err(e)` unchanged.
         *
         * @tparam F the transform callable
         * @param f callable applied to the success value, returning `U`
         * @return `Result<U, E>`
         */
        template <typename F>
            requires detail::MapCallable<F, T>
        [[nodiscard]] auto map(F &&f) const
            -> Result<detail::map_result_t<F, T>, E>
            requires detail::ValidResultTypes<detail::map_result_t<F, T>, E> &&
                     (!std::is_same_v<detail::map_result_t<F, T>, E>)
        {
            require_not_moved_();
            using U = detail::map_result_t<F, T>;

            if (is_ok())
            {
                if constexpr (std::is_void_v<U>)
                {
                    if constexpr (std::is_void_v<T>)
                        std::invoke(f);
                    else
                        std::invoke(f, ok_);
                    return Result<void, E>::Ok();
                }
                else
                {
                    if constexpr (std::is_void_v<T>)
                        return Result<U, E>::Ok(std::invoke(f));
                    else
                        return Result<U, E>::Ok(std::invoke(f, ok_));
                }
            }
            else
                return Result<U, E>::Err(err_);
        }

        /**
         * @brief Transforms the error value (MapErr).
         *
         * On Err(e), returns `Err(f(e))`; on Ok, returns `Ok` unchanged (`Ok()` when `T = void`).
         *
         * @tparam F the transform callable
         * @param f callable taking the error as `const E&` and returning a non-void `G`
         * @return `Result<T, G>`
         * @note A callable whose result is `void` is rejected (it would form the illegal
         *       `Result<T, void>`).
         */
        template <typename F>
            requires std::invocable<F, const E &> &&
                     (!std::is_void_v<std::invoke_result_t<F, const E &>>)
        [[nodiscard]] auto map_err(F &&f) const
            -> Result<T, detail::map_err_result_t<F, E>>
            requires detail::ValidResultTypes<T, detail::map_err_result_t<F, E>> &&
                     (!std::is_same_v<detail::map_err_result_t<F, E>, T>)
        {
            require_not_moved_();
            using E2 = detail::map_err_result_t<F, E>;

            if (is_err())
                return Result<T, E2>::Err(std::invoke(f, err_));
            else
            {
                if constexpr (std::is_void_v<T>)
                    return Result<void, E2>::Ok();
                else
                    return Result<T, E2>::Ok(ok_);
            }
        }

        /**
         * @brief Returns `f(success value)` if Ok, otherwise the provided default.
         *
         * Unlike `map`, this collapses to a plain value `U` rather than a `Result`.
         * When `T = void`, `f()` is called.
         *
         * @tparam F callable producing `U` from the success value (or nullary when `T = void`)
         * @param def value returned when in the Err state
         * @param f transform applied to the success value
         * @return `f(...)` if Ok, otherwise @p def
         * @throws bad_result_access when the result is in the Moved state
         */
        template <typename F>
            requires detail::MapCallable<F, T> && (!std::is_void_v<detail::map_result_t<F, T>>)
        [[nodiscard]] detail::map_result_t<F, T> map_or(detail::map_result_t<F, T> def, F &&f) const
        {
            require_not_moved_();
            if (is_ok())
            {
                if constexpr (std::is_void_v<T>)
                    return std::invoke(f);
                else
                    return std::invoke(f, ok_);
            }
            return def;
        }

        /**
         * @brief Returns `f(success value)` if Ok, otherwise `d(error)`.
         *
         * Collapses to a plain value `U`. When `T = void`, `f()` is called.
         *
         * @tparam D callable producing `U` from the error (bound as `const E&`)
         * @tparam F callable producing `U` from the success value (or nullary when `T = void`)
         * @param d fallback applied to the error
         * @param f transform applied to the success value
         * @return `f(...)` if Ok, otherwise `d(error)`
         */
        template <typename D, typename F>
            requires detail::MapCallable<F, T> && std::invocable<D, const E &> &&
                     (!std::is_void_v<detail::map_result_t<F, T>>) &&
                     std::convertible_to<detail::map_err_result_t<D, E>,
                                         detail::map_result_t<F, T>>
        [[nodiscard]] detail::map_result_t<F, T> map_or_else(D &&d, F &&f) const
        {
            require_not_moved_();
            using U = detail::map_result_t<F, T>;
            if (is_ok())
            {
                if constexpr (std::is_void_v<T>)
                    return std::invoke(f);
                else
                    return std::invoke(f, ok_);
            }
            return static_cast<U>(std::invoke(d, err_));
        }

        /**
         * @brief Invokes @p f on the success value if Ok, then returns `*this` unchanged.
         *
         * Useful for side effects (e.g. logging) in a chain. When `T = void`, `f()` is called.
         *
         * @tparam F callable observing the success value (or nullary when `T = void`)
         * @param f the observer
         * @return const reference to `*this`
         * @throws bad_result_access when the result is in the Moved state
         * @warning Returns a reference to `*this`; do not call on a temporary and keep the result.
         */
        template <typename F>
            requires detail::MapCallable<F, T>
        const Result &inspect(F &&f) const &
        {
            require_not_moved_();
            if (is_ok())
            {
                if constexpr (std::is_void_v<T>)
                    std::invoke(f);
                else
                    std::invoke(f, ok_);
            }
            return *this;
        }

        /**
         * @brief Invokes @p f on the success value if Ok, then returns `*this` by value.
         *
         * The rvalue counterpart of the `const &` overload: the observer runs before
         * the object is moved out, so the result stays valid when chained from a
         * temporary. When `T = void`, `f()` is called.
         *
         * @tparam F callable observing the success value (or nullary when `T = void`)
         * @param f the observer
         * @return `*this` moved into a new `Result`
         * @throws bad_result_access when the result is in the Moved state
         */
        template <typename F>
            requires detail::MapCallable<F, T>
        [[nodiscard]] Result inspect(F &&f) &&
        {
            require_not_moved_();
            if (is_ok())
            {
                if constexpr (std::is_void_v<T>)
                    std::invoke(f);
                else
                    std::invoke(f, ok_);
            }
            return std::move(*this);
        }

        /**
         * @brief Invokes @p f on the error value if Err, then returns `*this` unchanged.
         *
         * @tparam F callable observing the error value
         * @param f the observer
         * @return const reference to `*this`
         * @throws bad_result_access when the result is in the Moved state
         * @warning Returns a reference to `*this`; do not call on a temporary and keep the result.
         */
        template <typename F>
            requires std::invocable<F, const E &>
        const Result &inspect_err(F &&f) const &
        {
            require_not_moved_();
            if (is_err())
                std::invoke(f, err_);
            return *this;
        }

        /**
         * @brief Invokes @p f on the error value if Err, then returns `*this` by value.
         *
         * The rvalue counterpart of the `const &` overload: the observer runs before
         * the object is moved out, so the result stays valid when chained from a
         * temporary.
         *
         * @tparam F callable observing the error value
         * @param f the observer
         * @return `*this` moved into a new `Result`
         * @throws bad_result_access when the result is in the Moved state
         */
        template <typename F>
            requires std::invocable<F, const E &>
        [[nodiscard]] Result inspect_err(F &&f) &&
        {
            require_not_moved_();
            if (is_err())
                std::invoke(f, err_);
            return std::move(*this);
        }

        /**
         * @brief Attaches an error context message (const overload).
         *
         * On Ok, returns the success value unchanged without constructing any context
         * (zero-cost). On Err(e), returns `Err(Context<E>(e).context(msg))`, or appends
         * @p msg to the existing chain when `E` is already a `Context`.
         *
         * @tparam M message type constructible into `std::string`
         * @param msg description of what was being done
         * @return `Result<T, Context<E>>` (or `Result<T, E>` when `E` is a `Context`)
         */
        template <typename M>
            requires std::copy_constructible<E> &&
                     std::constructible_from<std::string, M &&>
        [[nodiscard]] auto context(M &&msg) const &
            -> Result<T, detail::context_error_t<E>>
        {
            require_not_moved_();
            using E2 = detail::context_error_t<E>;

            if (is_err())
            {
                if constexpr (detail::is_context_v<E>)
                    return Result<T, E2>::Err(err_.context(std::forward<M>(msg)));
                else
                    return Result<T, E2>::Err(Context<E>(err_).context(std::forward<M>(msg)));
            }
            if constexpr (std::is_void_v<T>)
                return Result<T, E2>::Ok();
            else
                return Result<T, E2>::Ok(ok_);
        }

        /// @overload (rvalue: moves the error instead of copying it)
        template <typename M>
            requires std::constructible_from<std::string, M &&>
        [[nodiscard]] auto context(M &&msg) &&
            -> Result<T, detail::context_error_t<E>>
        {
            require_not_moved_();
            using E2 = detail::context_error_t<E>;

            if (is_err())
            {
                if constexpr (detail::is_context_v<E>)
                    return Result<T, E2>::Err(std::move(err_).context(std::forward<M>(msg)));
                else
                    return Result<T, E2>::Err(Context<E>(std::move(err_)).context(std::forward<M>(msg)));
            }
            if constexpr (std::is_void_v<T>)
                return Result<T, E2>::Ok();
            else
                return Result<T, E2>::Ok(std::move(ok_));
        }

        /**
         * @brief Attaches a lazily-produced error context message (const overload).
         *
         * @p f is invoked at most once and only in the Err state, so no message is
         * produced for a successful result.
         *
         * @tparam F nullary callable returning a message constructible into `std::string`
         * @param f context producer, invoked only on Err
         * @return `Result<T, Context<E>>` (or `Result<T, E>` when `E` is a `Context`)
         */
        template <typename F>
            requires std::copy_constructible<E> && std::invocable<F> &&
                     std::constructible_from<std::string, std::invoke_result_t<F>>
        [[nodiscard]] auto with_context(F &&f) const &
            -> Result<T, detail::context_error_t<E>>
        {
            require_not_moved_();
            using E2 = detail::context_error_t<E>;

            if (is_err())
            {
                if constexpr (detail::is_context_v<E>)
                    return Result<T, E2>::Err(err_.context(std::invoke(std::forward<F>(f))));
                else
                    return Result<T, E2>::Err(Context<E>(err_).context(std::invoke(std::forward<F>(f))));
            }
            if constexpr (std::is_void_v<T>)
                return Result<T, E2>::Ok();
            else
                return Result<T, E2>::Ok(ok_);
        }

        /// @overload (rvalue: moves the error instead of copying it)
        template <typename F>
            requires std::invocable<F> &&
                     std::constructible_from<std::string, std::invoke_result_t<F>>
        [[nodiscard]] auto with_context(F &&f) &&
            -> Result<T, detail::context_error_t<E>>
        {
            require_not_moved_();
            using E2 = detail::context_error_t<E>;

            if (is_err())
            {
                if constexpr (detail::is_context_v<E>)
                    return Result<T, E2>::Err(std::move(err_).context(std::invoke(std::forward<F>(f))));
                else
                    return Result<T, E2>::Err(Context<E>(std::move(err_)).context(std::invoke(std::forward<F>(f))));
            }
            if constexpr (std::is_void_v<T>)
                return Result<T, E2>::Ok();
            else
                return Result<T, E2>::Ok(std::move(ok_));
        }

    public:
        /**
         * @brief Chains a fallible operation (FlatMap / AndThen).
         *
         * On Ok, invokes `f` (`f()` when `T = void`, otherwise `f(success value)`; `f` must
         * return a `Result`); on Err(e), short-circuits and returns `Err(e)`.
         *
         * @tparam F callable returning a `Result`
         * @param f the follow-up operation
         * @return the return type of `f` (a `Result`)
         */
        template <typename F>
            requires detail::CrefResultFn<F, T>
        auto and_then(F &&f) const & -> detail::cref_result_t<F, T>
        {
            require_not_moved_();
            using Ret = detail::cref_result_t<F, T>;
            if (is_ok())
            {
                if constexpr (std::is_void_v<T>)
                    return std::invoke(f);
                else
                    return std::invoke(f, ok_);
            }
            else
                return Ret::Err(err_);
        }

        /**
         * @brief Chains a fallible operation (FlatMap / AndThen), rvalue/move overload.
         *
         * @tparam F callable returning a `Result`
         * @param f the follow-up operation
         * @return the return type of `f` (a `Result`)
         */
        template <typename F>
            requires detail::ValueResultFn<F, T>
        auto and_then(F &&f) && -> detail::value_result_t<F, T>
        {
            require_not_moved_();
            using Ret = detail::value_result_t<F, T>;
            if (is_ok())
            {
                if constexpr (std::is_void_v<T>)
                    return std::invoke(f);
                else
                    return std::invoke(f, std::move(ok_));
            }
            else
                return Ret::Err(std::move(err_));
        }

        /**
         * @brief Chains a fallible recovery (OrElse).
         *
         * On Err(e), invokes `f(e)` (which must return a `Result`); on Ok, passes the
         * success value through unchanged (`Ok()` when `T = void`). Mirror image of `and_then`.
         *
         * @tparam F callable taking `E` and returning a `Result`
         * @param f the recovery operation
         * @return the return type of `f` (a `Result`)
         */
        template <typename F>
            requires detail::ResultType<std::invoke_result_t<F, const E &>>
        auto or_else(F &&f) const & -> std::invoke_result_t<F, const E &>
        {
            require_not_moved_();
            using Ret = std::invoke_result_t<F, const E &>;
            if (is_err())
                return std::invoke(f, err_);
            if constexpr (std::is_void_v<T>)
                return Ret::Ok();
            else
                return Ret::Ok(ok_);
        }

        /**
         * @brief Chains a fallible recovery (OrElse), rvalue/move overload.
         *
         * @tparam F callable taking `E` and returning a `Result`
         * @param f the recovery operation
         * @return the return type of `f` (a `Result`)
         */
        template <typename F>
            requires detail::ResultType<std::invoke_result_t<F, E>>
        auto or_else(F &&f) && -> std::invoke_result_t<F, E>
        {
            require_not_moved_();
            using Ret = std::invoke_result_t<F, E>;
            if (is_err())
                return std::invoke(f, std::move(err_));
            if constexpr (std::is_void_v<T>)
                return Ret::Ok();
            else
                return Ret::Ok(std::move(ok_));
        }

        /**
         * @brief Returns @p other if `*this` is Ok, otherwise propagates this error.
         *
         * The eager counterpart of `and_then` (Rust `Result::and`): no closure is
         * involved, so @p other is evaluated unconditionally. The success type becomes
         * `U` (that of @p other), while the error type `E` is preserved.
         *
         * @tparam U success type of @p other
         * @param other the result yielded when `*this` is Ok
         * @return @p other if Ok, otherwise `Err(e)` carrying this result's error
         */
        template <typename U>
            requires detail::ValidResultTypes<U, E>
        [[nodiscard]] Result<U, E> and_with(Result<U, E> other) const &
        {
            require_not_moved_();
            if (is_ok())
                return other;
            return Result<U, E>::Err(err_);
        }

        /// @overload (rvalue: moves the error when `*this` is Err)
        template <typename U>
            requires detail::ValidResultTypes<U, E>
        [[nodiscard]] Result<U, E> and_with(Result<U, E> other) &&
        {
            require_not_moved_();
            if (is_ok())
                return other;
            return Result<U, E>::Err(std::move(err_));
        }

        /**
         * @brief Returns `*this` if Ok, otherwise @p other.
         *
         * The eager counterpart of `or_else` (Rust `Result::or`): no closure is
         * involved, so @p other is evaluated unconditionally. The success type `T` is
         * preserved, while the error type becomes `F` (that of @p other).
         *
         * @tparam F error type of @p other (may differ from this result's error type)
         * @param other the result yielded when `*this` is Err
         * @return `*this` if Ok, otherwise @p other
         */
        template <typename F>
            requires detail::ValidResultTypes<T, F> && detail::NotResult<F>
        [[nodiscard]] Result<T, F> or_with(Result<T, F> other) const &
        {
            require_not_moved_();
            if (is_err())
                return other;
            if constexpr (std::is_void_v<T>)
                return Result<T, F>::Ok();
            else
                return Result<T, F>::Ok(ok_);
        }

        /// @overload (rvalue: moves the success value when `*this` is Ok)
        template <typename F>
            requires detail::ValidResultTypes<T, F> && detail::NotResult<F>
        [[nodiscard]] Result<T, F> or_with(Result<T, F> other) &&
        {
            require_not_moved_();
            if (is_err())
                return other;
            if constexpr (std::is_void_v<T>)
                return Result<T, F>::Ok();
            else
                return Result<T, F>::Ok(std::move(ok_));
        }

        /**
         * @brief Flattens one level of nesting.
         *
         * On `Ok(inner)`, returns `inner`; on `Err(e)`, returns `Err(e)`.
         * Available only when the success type is itself a `Result<U, E>` with the
         * same error type.
         *
         * @tparam U Result value type (inferred from T being a Result)
         * @return the flattened `Result<U, E>`
         */
        template <typename U = T>
            requires detail::ResultType<U> &&
                     std::same_as<detail::result_error_t<U>, E>
        [[nodiscard]] auto flatten() const & -> Result<detail::result_value_t<U>, E>
        {
            require_not_moved_();
            if (is_ok())
                return ok_;
            return Result<detail::result_value_t<U>, E>::Err(err_);
        }

        /// @overload
        template <typename U = T>
            requires detail::ResultType<U> &&
                     std::same_as<detail::result_error_t<U>, E>
        [[nodiscard]] auto flatten()
            && -> Result<detail::result_value_t<U>, E>
        {
            require_not_moved_();
            if (is_ok())
            {
                auto inner = std::move(ok_);
                destroy_();
                tag_ = detail::Tag::Moved;
                return inner;
            }
            auto e = std::move(err_);
            destroy_();
            tag_ = detail::Tag::Moved;
            return Result<detail::result_value_t<U>, E>::Err(std::move(e));
        }

        /**
         * @brief Converts to `Option<T>`, discarding any error (Rust `Result::ok`).
         *
         * Ok becomes `Some` (`Some()` when `T = void`); Err becomes `None`. The `const&`
         * overload copies the success value and leaves `*this` unchanged; the `&&`
         * overload moves it out and leaves `*this` in the Moved state.
         *
         * @return `Some(value)` when Ok, otherwise `None`
         * @throws bad_result_access when `*this` is in the Moved state
         */
        [[nodiscard]] Option<T> ok() const &;
        /// @overload (rvalue: moves the value out)
        [[nodiscard]] Option<T> ok() &&;

        /**
         * @brief Converts to `Option<E>`, discarding any success value (Rust `Result::err`).
         *
         * Err becomes `Some`; Ok becomes `None`. The `const&` overload copies the error
         * and leaves `*this` unchanged; the `&&` overload moves it out and leaves
         * `*this` in the Moved state.
         *
         * @return `Some(error)` when Err, otherwise `None`
         * @throws bad_result_access when `*this` is in the Moved state
         */
        [[nodiscard]] Option<E> err() const &;
        /// @overload (rvalue: moves the error out)
        [[nodiscard]] Option<E> err() &&;

        /**
         * @brief Transposes a `Result<Option<U>, E>` into `Option<Result<U, E>>`.
         *
         * `Ok(Some(u))` becomes `Some(Ok(u))`, `Ok(None)` becomes `None`,
         * `Err(e)` becomes `Some(Err(e))`.
         *
         * @tparam V Option type (inferred from T being an Option)
         * @return the transposed `Option`
         */
        template <typename V = T>
            requires detail::OptionType<V>
        [[nodiscard]] auto transpose() const &
            -> Option<Result<typename V::value_type, E>>
        {
            require_not_moved_();
            using InnerV = typename V::value_type;
            using Out = Option<Result<InnerV, E>>;

            if (is_ok())
            {
                if (ok_.is_some())
                {
                    if constexpr (std::is_void_v<InnerV>)
                        return Out::Some(Result<void, E>::Ok());
                    else
                        return Out::Some(Result<InnerV, E>::Ok(ok_.unwrap()));
                }
                return Out::None();
            }
            return Out::Some(Result<InnerV, E>::Err(err_));
        }

        /// @overload
        template <typename V = T>
            requires detail::OptionType<V>
        [[nodiscard]] auto transpose() &&
            -> Option<Result<typename V::value_type, E>>
        {
            require_not_moved_();
            using InnerV = typename V::value_type;
            using Out = Option<Result<InnerV, E>>;

            if (is_ok())
            {
                if (ok_.is_some())
                {
                    if constexpr (std::is_void_v<InnerV>)
                    {
                        std::move(ok_).unwrap();
                        destroy_();
                        tag_ = detail::Tag::Moved;
                        return Out::Some(Result<void, E>::Ok());
                    }
                    else
                    {
                        auto v = std::move(ok_).unwrap();
                        destroy_();
                        tag_ = detail::Tag::Moved;
                        return Out::Some(Result<InnerV, E>::Ok(std::move(v)));
                    }
                }
                destroy_();
                tag_ = detail::Tag::Moved;
                return Out::None();
            }
            auto e = std::move(err_);
            destroy_();
            tag_ = detail::Tag::Moved;
            return Out::Some(Result<InnerV, E>::Err(std::move(e)));
        }

    public:
        /**
         * @brief Equality comparison.
         *
         * Two results are equal iff they hold the same alternative with equal contents:
         * both Ok with equal success values (or both Ok when `T = void`), or both Err with
         * equal error values.
         *
         * Available only when the stored types are equality-comparable. `operator!=` is
         * synthesized by the compiler (C++20).
         *
         * @param a left operand
         * @param b right operand
         * @return whether @p a and @p b are equal
         */
        friend bool operator==(const Result &a, const Result &b)
            requires(std::is_void_v<T> || std::equality_comparable<OkT>) &&
                    std::equality_comparable<E>
        {
            if (a.tag_ == detail::Tag::Moved || b.tag_ == detail::Tag::Moved)
                throw bad_result_access("Result comparison after move");
            if (a.tag_ != b.tag_)
                return false;
            if (a.is_ok())
            {
                if constexpr (std::is_void_v<T>)
                    return true;
                else
                    return a.ok_ == b.ok_;
            }
            return a.err_ == b.err_;
        }
    };

    namespace detail
    {
        /// @brief Trait specialization exposing the value/error types of a `Result`.
        template <typename T, typename E>
        struct result_traits<Result<T, E>>
        {
            using value_type = T;
            using error_type = E;
        };
    }
}

#endif // INCLUDE_PJH_RESULT_RESULT_HPP
