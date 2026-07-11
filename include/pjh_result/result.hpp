/**
 * @file result.hpp
 * @brief Rust-style `Result<T, E>` monad backed by hand-written tagged-union storage.
 */
#ifndef INCLUDE_PJH_RESULT_RESULT_HPP
#define INCLUDE_PJH_RESULT_RESULT_HPP

#include <concepts>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

#include "pjh_result/detail/traits.hpp"
#include "pjh_result/errors.hpp"

namespace pjh::result
{
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

        /// @brief Result type of `map`: `f()` when `T = void`, otherwise `f(T)`.
        ///        Evaluated lazily to avoid forming a `void` argument.
        template <typename F, typename T, bool = std::is_void_v<T>>
        struct map_result
        {
            using type = std::invoke_result_t<F, T>;
        };
        template <typename F, typename T>
        struct map_result<F, T, true>
        {
            using type = std::invoke_result_t<F>;
        };
        template <typename F, typename T>
        using map_result_t = typename map_result<F, T>::type;

        /// @brief Result type of `and_then(const &)`: `f()` when `T = void`, otherwise `f(const T&)`.
        template <typename F, typename T, bool = std::is_void_v<T>>
        struct cref_result
        {
            using type = std::invoke_result_t<F, const T &>;
        };
        template <typename F, typename T>
        struct cref_result<F, T, true>
        {
            using type = std::invoke_result_t<F>;
        };
        template <typename F, typename T>
        using cref_result_t = typename cref_result<F, T>::type;

        /// @brief Result type of `and_then(&&)`: `f()` when `T = void`, otherwise `f(T)`.
        template <typename F, typename T, bool = std::is_void_v<T>>
        struct value_result
        {
            using type = std::invoke_result_t<F, T>;
        };
        template <typename F, typename T>
        struct value_result<F, T, true>
        {
            using type = std::invoke_result_t<F>;
        };
        template <typename F, typename T>
        using value_result_t = typename value_result<F, T>::type;

        /// @brief Whether `f` is callable on the success value: requires `f()` when `T = void`, else `f(T)`.
        template <typename F, typename T>
        concept MapCallable = (std::is_void_v<T> && std::invocable<F>) ||
                              (!std::is_void_v<T> && std::invocable<F, T>);

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
        ///        (assumes this object's storage is empty / already destroyed).
        void construct_from_(Result &&o) noexcept
        {
            tag_ = o.tag_;
            if (tag_ == detail::Tag::Ok)
                ::new (static_cast<void *>(std::addressof(ok_))) OkT(std::move(o.ok_));
            else
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
        Result(const Result &o)
            requires std::copy_constructible<OkT> && std::copy_constructible<E>
            : tag_(o.tag_)
        {
            if (tag_ == detail::Tag::Ok)
                ::new (static_cast<void *>(std::addressof(ok_))) OkT(o.ok_);
            else
                ::new (static_cast<void *>(std::addressof(err_))) E(o.err_);
        }

        /// @brief Move constructor: nothrow-moves the other object's active member.
        Result(Result &&o) noexcept : tag_(o.tag_)
        {
            if (tag_ == detail::Tag::Ok)
                ::new (static_cast<void *>(std::addressof(ok_))) OkT(std::move(o.ok_));
            else
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

        /**
         * @brief Whether the result is Ok and the success value satisfies @p f.
         *
         * When `T = void`, @p f is invoked with no argument.
         *
         * @tparam F predicate on the success value (or nullary when `T = void`)
         * @param f the predicate
         * @return `is_ok() && bool(f(...))`
         */
        template <typename F>
            requires detail::MapCallable<F, T>
        [[nodiscard]] bool is_ok_and(F &&f) const
        {
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
         * @tparam F predicate on the error value
         * @param f the predicate
         * @return `is_err() && bool(f(error))`
         */
        template <typename F>
            requires std::invocable<F, E>
        [[nodiscard]] bool is_err_and(F &&f) const
        {
            return is_err() && static_cast<bool>(std::invoke(f, err_));
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
        void unwrap() const
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
         */
        [[nodiscard]] OkT unwrap_or(OkT val) const
            requires(!std::is_void_v<T>)
        {
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
        void expect(const std::string &msg) const
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
         */
        [[nodiscard]] T unwrap_or_default() const
            requires(!std::is_void_v<T>) && std::default_initializable<T>
        {
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
         */
        [[nodiscard]] E unwrap_err_or(E err) const
        {
            return is_err() ? err_ : std::move(err);
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
         * @param f callable taking `E` and returning `G`
         * @return `Result<T, G>`
         */
        template <typename F>
            requires std::invocable<F, E>
        [[nodiscard]] auto map_err(F &&f) const
            -> Result<T, std::invoke_result_t<F, E>>
            requires detail::ValidResultTypes<T, std::invoke_result_t<F, E>> &&
                     (!std::is_same_v<std::invoke_result_t<F, E>, T>)
        {
            require_not_moved_();
            using E2 = std::invoke_result_t<F, E>;

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
         */
        template <typename F>
            requires detail::MapCallable<F, T> && (!std::is_void_v<detail::map_result_t<F, T>>)
        [[nodiscard]] detail::map_result_t<F, T> map_or(detail::map_result_t<F, T> def, F &&f) const
        {
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
         * @tparam D callable producing `U` from the error
         * @tparam F callable producing `U` from the success value (or nullary when `T = void`)
         * @param d fallback applied to the error
         * @param f transform applied to the success value
         * @return `f(...)` if Ok, otherwise `d(error)`
         */
        template <typename D, typename F>
            requires detail::MapCallable<F, T> && std::invocable<D, E> &&
                     (!std::is_void_v<detail::map_result_t<F, T>>) &&
                     std::convertible_to<std::invoke_result_t<D, E>, detail::map_result_t<F, T>>
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
         * @warning Returns a reference to `*this`; do not call on a temporary and keep the result.
         */
        template <typename F>
            requires detail::MapCallable<F, T>
        const Result &inspect(F &&f) const &
        {
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
         * @brief Invokes @p f on the error value if Err, then returns `*this` unchanged.
         *
         * @tparam F callable observing the error value
         * @param f the observer
         * @return const reference to `*this`
         * @warning Returns a reference to `*this`; do not call on a temporary and keep the result.
         */
        template <typename F>
            requires std::invocable<F, const E &>
        const Result &inspect_err(F &&f) const &
        {
            if (is_err())
                std::invoke(f, err_);
            return *this;
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
            if (is_ok())
            {
                if constexpr (std::is_void_v<detail::result_value_t<U>>)
                    return Result<void, E>::Ok();
                else
                    return ok_;
            }
            return Result<detail::result_value_t<U>, E>::Err(err_);
        }

        /// @overload
        template <typename U = T>
            requires detail::ResultType<U> &&
                     std::same_as<detail::result_error_t<U>, E>
        [[nodiscard]] auto flatten()
            && -> Result<detail::result_value_t<U>, E>
        {
            if (is_ok())
            {
                auto inner = std::move(ok_);
                tag_ = detail::Tag::Moved;
                return inner;
            }
            auto e = std::move(err_);
            tag_ = detail::Tag::Moved;
            return Result<detail::result_value_t<U>, E>::Err(std::move(e));
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
