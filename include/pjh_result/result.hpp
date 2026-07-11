#ifndef INCLUDE_PJH_RESULT_RESULT_HPP
#define INCLUDE_PJH_RESULT_RESULT_HPP

#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "pjh_result/detail/traits.hpp"
#include "pjh_result/errors.hpp"

namespace pjh::result
{
    namespace utils
    {
        /// @brief void 结果的占位类型：T = void 时成功分支存储它。
        struct Unit
        {
        };

        namespace detail
        {
            /// @brief 标记 Result 当前存储的是哪一路（成功值或错误值）。
            enum class Tag : unsigned char
            {
                Ok,
                Err
            };

            /// @brief map 的结果类型：T = void 时用 f()，否则用 f(T)。惰性求值以避免 void 实参。
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

            /// @brief and_then(const &) 的结果类型：T = void 时用 f()，否则用 f(const T&)。
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

            /// @brief and_then(&&) 的结果类型：T = void 时用 f()，否则用 f(T)。
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

            /// @brief f 是否可作用于成功值：T = void 要求 f() 可调用，否则 f(T)。
            template <typename F, typename T>
            concept MapCallable = (std::is_void_v<T> && std::invocable<F>) ||
                                  (!std::is_void_v<T> && std::invocable<F, T>);

            /// @brief f 作用于 const 成功值后是否返回 Result。
            template <typename F, typename T>
            concept CrefResultFn =
                (std::is_void_v<T> && result_helper::ResultType<std::invoke_result_t<F>>) ||
                (!std::is_void_v<T> && result_helper::ResultType<std::invoke_result_t<F, const T &>>);

            /// @brief f 作用于成功值（移动）后是否返回 Result。
            template <typename F, typename T>
            concept ValueResultFn =
                (std::is_void_v<T> && result_helper::ResultType<std::invoke_result_t<F>>) ||
                (!std::is_void_v<T> && result_helper::ResultType<std::invoke_result_t<F, T>>);
        }

        /**
         * @brief 用于在宏中擦除 T 类型，触发 Result 的隐式转换并自动构建 Err 状态。
         *
         * @tparam E 错误值类型
         */
        template <typename E>
        struct Failure
        {
            E error;
        };

        /// @brief 推导指引：允许直接写 `Failure{err}` 而无需 `Failure<E>{err}`。
        template <typename E>
        Failure(E) -> Failure<E>;

        /**
         * @brief 结果封装类 (Monad)。
         *
         * 提供类似 Rust 的 `Result<T, E>` 机制，强制调用者处理错误。
         * 它要么包含一个成功的值 T，要么包含一个错误 E，二者必居其一。
         *
         * 内部使用手写的带标签联合体（tagged union）存储，不依赖 `std::variant`：
         * - 无 `valueless_by_exception` 之类的“第三态”，`is_ok()`/`is_err()` 恒互补；
         * - 访问不经 `std::get` 的运行时校验，直接读取活跃成员。
         *
         * 支持 `T = void`：此时成功分支不携带值，`Ok()` 无参构造，`unwrap()` 返回 void，
         * 且不提供 `unwrap_or`。
         *
         * @tparam T 成功时的数据类型（可为 void）
         * @tparam E 失败时的错误类型
         *
         * @pre T（非 void 时）与 E 的移动构造必须为 `noexcept`（见类内 static_assert）。
         *      这样赋值可实现为“析构旧值 + 无异常地移动构造新值”，保证对象永不进入无效状态。
         * @note 禁止忽略返回值 ([[nodiscard]])。
         * @note 禁止 T 与 E 为同一类型。
         */
        template <typename T, typename E>
            requires result_helper::ValidResultTypes<T, E> &&
                     result_helper::NotResult<T> &&
                     result_helper::NotResult<E>
        class [[nodiscard]] Result
        {
        private:
            /// @brief 成功分支的实际存储类型：T = void 时退化为 Unit。
            using OkT = std::conditional_t<std::is_void_v<T>, Unit, T>;

            static_assert(std::is_nothrow_move_constructible_v<OkT>,
                          "pjh::result::Result 要求 T 的移动构造为 noexcept");
            static_assert(std::is_nothrow_move_constructible_v<E>,
                          "pjh::result::Result 要求 E 的移动构造为 noexcept");

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

            /// @brief 就地构造 Ok 分支（成功值由 a... 转发构造，无实参时值初始化）。
            template <typename... A>
                requires std::constructible_from<OkT, A &&...>
            explicit Result(ok_t, A &&...a) noexcept(std::is_nothrow_constructible_v<OkT, A &&...>)
                : tag_(detail::Tag::Ok), ok_(std::forward<A>(a)...)
            {
            }

            /// @brief 就地构造 Err 分支（错误值由 a... 转发构造）。
            template <typename... A>
                requires std::constructible_from<E, A &&...>
            explicit Result(err_t, A &&...a) noexcept(std::is_nothrow_constructible_v<E, A &&...>)
                : tag_(detail::Tag::Err), err_(std::forward<A>(a)...)
            {
            }

            /// @brief 析构当前活跃成员；平凡可析构类型下为空操作。
            void destroy_() noexcept
            {
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

            /// @brief 从右值 Result 无异常地移动构造本对象的活跃成员（假定本对象存储为空/已析构）。
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
             * @brief 构造成功结果 Ok(val)。仅当 T 非 void 时可用。
             *
             * @tparam U 用于构造 T 的实参类型
             * @param val 成功值
             * @return Result 处于 Ok 状态的结果
             */
            template <typename U>
                requires(!std::is_void_v<T>) && std::constructible_from<OkT, U &&>
            static Result Ok(U &&val) noexcept(std::is_nothrow_constructible_v<OkT, U &&>)
            {
                return Result(ok_t{}, std::forward<U>(val));
            }

            /**
             * @brief 构造成功结果 Ok()。仅当 T 为 void 时可用。
             *
             * @return Result 处于 Ok 状态的结果
             */
            static Result Ok() noexcept
                requires std::is_void_v<T>
            {
                return Result(ok_t{});
            }

            /**
             * @brief 构造错误结果 Err(err)。
             *
             * @tparam G 用于构造 E 的实参类型
             * @param err 错误值
             * @return Result 处于 Err 状态的结果
             */
            template <typename G>
                requires std::constructible_from<E, G &&>
            static Result Err(G &&err) noexcept(std::is_nothrow_constructible_v<E, G &&>)
            {
                return Result(err_t{}, std::forward<G>(err));
            }

        public:
            /// @brief 拷贝构造：复制对方的活跃成员。
            Result(const Result &o)
                requires std::copy_constructible<OkT> && std::copy_constructible<E>
                : tag_(o.tag_)
            {
                if (tag_ == detail::Tag::Ok)
                    ::new (static_cast<void *>(std::addressof(ok_))) OkT(o.ok_);
                else
                    ::new (static_cast<void *>(std::addressof(err_))) E(o.err_);
            }

            /// @brief 移动构造：无异常地移动对方的活跃成员。
            Result(Result &&o) noexcept : tag_(o.tag_)
            {
                if (tag_ == detail::Tag::Ok)
                    ::new (static_cast<void *>(std::addressof(ok_))) OkT(std::move(o.ok_));
                else
                    ::new (static_cast<void *>(std::addressof(err_))) E(std::move(o.err_));
            }

            /**
             * @brief 拷贝赋值（强异常保证）。
             *
             * 先拷贝构造临时对象（此步若抛出，`*this` 保持不变），再析构旧值并无异常地移动构造。
             * 因此本对象永不进入无效状态。
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

            /// @brief 移动赋值：析构旧值后无异常地移动构造。
            Result &operator=(Result &&o) noexcept
            {
                if (this != std::addressof(o))
                {
                    destroy_();
                    construct_from_(std::move(o));
                }
                return *this;
            }

            /// @brief 析构：销毁当前活跃成员。
            ~Result() { destroy_(); }

            /**
             * @brief 由 Failure 隐式构造 Err 结果（拷贝错误）。
             *
             * @tparam G Failure 携带的错误类型
             * @param f 错误载体
             */
            template <typename G>
                requires std::constructible_from<E, const G &>
            Result(const Failure<G> &f) noexcept(std::is_nothrow_constructible_v<E, const G &>)
                : tag_(detail::Tag::Err), err_(f.error)
            {
            }

            /**
             * @brief 由 Failure 隐式构造 Err 结果（移动错误）。
             *
             * @tparam G Failure 携带的错误类型
             * @param f 错误载体
             */
            template <typename G>
                requires std::constructible_from<E, G &&>
            Result(Failure<G> &&f) noexcept(std::is_nothrow_constructible_v<E, G &&>)
                : tag_(detail::Tag::Err), err_(std::forward<G>(f.error))
            {
            }

        public:
            /// @brief 当前是否为 Ok 状态。
            bool is_ok() const noexcept { return tag_ == detail::Tag::Ok; }
            /// @brief 当前是否为 Err 状态。
            bool is_err() const noexcept { return tag_ == detail::Tag::Err; }

        public:
            /**
             * @brief 解包成功值；若为 Err 则抛出异常。仅当 T 非 void 时可用。
             *
             * @throws result_helper::bad_result_access 当前处于 Err 状态
             * @return T& 内部成功值的引用
             */
            [[nodiscard]] OkT &unwrap() &
                requires(!std::is_void_v<T>)
            {
                if (!is_ok())
                    throw result_helper::bad_result_access("Result::unwrap() called on Err");
                return ok_;
            }

            /**
             * @brief 解包成功值；若为 Err 则抛出异常。仅当 T 非 void 时可用。
             *
             * @throws result_helper::bad_result_access 当前处于 Err 状态
             * @return const T& 内部成功值的常量引用
             */
            [[nodiscard]] const OkT &unwrap() const &
                requires(!std::is_void_v<T>)
            {
                if (!is_ok())
                    throw result_helper::bad_result_access("Result::unwrap() called on Err");
                return ok_;
            }

            /**
             * @brief 解包并移出成功值；若为 Err 则抛出异常。仅当 T 非 void 时可用。
             *
             * @throws result_helper::bad_result_access 当前处于 Err 状态
             * @return T 移动得到的成功值
             */
            [[nodiscard]] OkT unwrap() &&
                requires(!std::is_void_v<T>)
            {
                if (!is_ok())
                    throw result_helper::bad_result_access("Result::unwrap() called on Err");
                return std::move(ok_);
            }

            /**
             * @brief 断言当前为 Ok；若为 Err 则抛出异常。仅当 T 为 void 时可用。
             *
             * @throws result_helper::bad_result_access 当前处于 Err 状态
             */
            void unwrap() const
                requires std::is_void_v<T>
            {
                if (is_err())
                    throw result_helper::bad_result_access("Result<void>::unwrap() called on Err");
            }

            /**
             * @brief 解包成功值；若为 Err 则返回给定默认值。仅当 T 非 void 时可用。
             *
             * @param val Err 时返回的默认值
             * @return T 成功值或默认值
             */
            [[nodiscard]] OkT unwrap_or(OkT val) const
                requires(!std::is_void_v<T>)
            {
                return is_ok() ? ok_ : std::move(val);
            }

            /**
             * @brief 解包错误值；若为 Ok 则抛出异常。
             *
             * @throws result_helper::bad_result_access 当前处于 Ok 状态
             * @return E& 内部错误值的引用
             */
            [[nodiscard]] E &unwrap_err() &
            {
                if (!is_err())
                    throw result_helper::bad_result_access("Result::unwrap_err() called on Ok");
                return err_;
            }

            /**
             * @brief 解包错误值；若为 Ok 则抛出异常。
             *
             * @throws result_helper::bad_result_access 当前处于 Ok 状态
             * @return const E& 内部错误值的常量引用
             */
            [[nodiscard]] const E &unwrap_err() const &
            {
                if (!is_err())
                    throw result_helper::bad_result_access("Result::unwrap_err() called on Ok");
                return err_;
            }

            /**
             * @brief 解包并移出错误值；若为 Ok 则抛出异常。
             *
             * @throws result_helper::bad_result_access 当前处于 Ok 状态
             * @return E 移动得到的错误值
             */
            [[nodiscard]] E unwrap_err() &&
            {
                if (!is_err())
                    throw result_helper::bad_result_access("Result::unwrap_err() called on Ok");
                return std::move(err_);
            }

            /**
             * @brief 解包错误值；若为 Ok 则返回给定默认值。
             *
             * @param err Ok 时返回的默认值
             * @return E 错误值或默认值
             */
            [[nodiscard]] E unwrap_err_or(E err) const
            {
                return is_err() ? err_ : std::move(err);
            }

        public:
            /**
             * @brief 转换成功值 (Map)。
             *
             * Ok 时对成功值应用 f（T = void 时调用 f()），返回 Ok(f(...))；Err(e) 时原样返回 Err(e)。
             *
             * @tparam F 转换函数类型
             * @param f 作用于成功值、返回 U 的可调用对象
             * @return Result<U, E> 新的结果类型
             */
            template <typename F>
                requires detail::MapCallable<F, T>
            [[nodiscard]] auto map(F &&f) const
                -> Result<detail::map_result_t<F, T>, E>
                requires result_helper::ValidResultTypes<detail::map_result_t<F, T>, E> &&
                         (!std::is_same_v<detail::map_result_t<F, T>, E>)
            {
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
             * @brief 转换错误值 (Map_Err)。
             *
             * Err(e) 时返回 Err(f(e))；Ok 时原样返回 Ok（T = void 时为 Ok()）。
             *
             * @tparam F 转换函数类型
             * @param f 接受 E、返回 G 的可调用对象
             * @return Result<T, G> 新的结果类型
             */
            template <typename F>
                requires std::invocable<F, E>
            [[nodiscard]] auto map_err(F &&f) const
                -> Result<T, std::invoke_result_t<F, E>>
                requires result_helper::ValidResultTypes<T, std::invoke_result_t<F, E>> &&
                         (!std::is_same_v<std::invoke_result_t<F, E>, T>)
            {
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

        public:
            /**
             * @brief 链式调用 (FlatMap / AndThen)。
             *
             * Ok 时调用 f（T = void 时 f()，否则 f(成功值)，f 必须返回一个 Result）；
             * Err(e) 时短路返回 Err(e)。
             *
             * @tparam F 返回 Result 的可调用对象
             * @param f 后续操作
             * @return f 的返回类型（一个 Result）
             */
            template <typename F>
                requires detail::CrefResultFn<F, T>
            auto and_then(F &&f) const & -> detail::cref_result_t<F, T>
            {
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
             * @brief 链式调用 (FlatMap / AndThen)，移动语义版本。
             *
             * @tparam F 返回 Result 的可调用对象
             * @param f 后续操作
             * @return f 的返回类型（一个 Result）
             */
            template <typename F>
                requires detail::ValueResultFn<F, T>
            auto and_then(F &&f) && -> detail::value_result_t<F, T>
            {
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
        };

        namespace result_helper
        {
            template <typename T, typename E>
            struct result_traits<Result<T, E>>
            {
                using value_type = T;
                using error_type = E;
            };
        }
    }
}

#endif // INCLUDE_PJH_RESULT_RESULT_HPP
