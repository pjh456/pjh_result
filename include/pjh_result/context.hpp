/**
 * @file context.hpp
 * @brief Error-context wrapper `Context<E>` used to attach "what was being done"
 *        messages while preserving the original error type.
 *
 * `Context<E>` is itself a valid error type: it can be stored in a `Result`, can be
 * given another context layer, and exposes the full message chain plus the root
 * cause. The chain is stored as a flat, copyable vector rather than a recursive
 * `optional<Context<E>>`, so no incomplete-type recursion is involved.
 */
#ifndef INCLUDE_PJH_RESULT_CONTEXT_HPP
#define INCLUDE_PJH_RESULT_CONTEXT_HPP

#include <concepts>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace pjh::result
{
    template <typename E>
    class Context;
}

namespace pjh::result::detail
{
    /// @brief Whether `E` (ignoring cv/ref) is already a `Context<...>`.
    template <typename E>
    struct is_context : std::false_type
    {
    };

    /// @overload
    template <typename X>
    struct is_context<Context<X>> : std::true_type
    {
    };

    /// @brief `is_context` evaluated on the decayed type of `E`.
    template <typename E>
    inline constexpr bool is_context_v = is_context<std::remove_cvref_t<E>>::value;

    /// @brief Error type produced by attaching a context: `Context<E>`, or `E` itself
    ///        when `E` is already a `Context`, so repeated calls stay flat.
    template <typename E>
    struct context_error
    {
        using type = Context<std::remove_cvref_t<E>>;
    };

    /// @overload
    template <typename X>
    struct context_error<Context<X>>
    {
        using type = Context<X>;
    };

    /// @brief `context_error` result type on the decayed type of `E`.
    template <typename E>
    using context_error_t = typename context_error<std::remove_cvref_t<E>>::type;
}

namespace pjh::result
{
    /**
     * @brief An error together with the chain of contexts that led to it.
     *
     * `cause_` holds the original error (`E`), preserving its type. `chain_` holds
     * the context messages in **outermost-first** order, i.e. the most recently
     * attached context comes first. Rendering outer-to-inner is therefore a plain
     * forward iteration over `messages()`:
     * @code
     * store.load("deck").context("parse json").context("load deck");
     * // messages() == { "load deck", "parse json" }, root_cause() == <IoError>
     * // rendered: "load deck: parse json: <root>"
     * @endcode
     *
     * @tparam E the root error type
     */
    template <typename E>
    class [[nodiscard]] Context
    {
    public:
        /// @brief The root error type carried by this context.
        using error_type = E;

        /**
         * @brief Wraps an existing error as the root cause, with no context message yet.
         *
         * @param cause the original error
         */
        explicit Context(E cause) : cause_(std::move(cause)) {}

        /// @brief Copy constructor; available whenever `E` is copy-constructible.
        Context(const Context &) = default;
        /// @brief Move constructor; `noexcept` exactly when `E`'s move is `noexcept`.
        Context(Context &&) noexcept(std::is_nothrow_move_constructible_v<E>) = default;
        /// @brief Copy assignment; available whenever `E` is copy-assignable.
        Context &operator=(const Context &) = default;
        /// @brief Move assignment; `noexcept` exactly when `E`'s move is `noexcept`.
        Context &operator=(Context &&) noexcept(std::is_nothrow_move_assignable_v<E>) = default;
        ~Context() = default;

        /**
         * @brief Appends one outer context layer.
         *
         * The message is materialized as a `std::string` only when this is called
         * (never on a successful `Result`).
         *
         * @tparam M message type constructible into `std::string`
         * @param msg the "what was being done" message
         * @return a copy of `*this` with @p msg attached as the outermost layer
         */
        template <typename M>
            requires std::copy_constructible<E> &&
                     std::constructible_from<std::string, M &&>
        [[nodiscard]] Context context(M &&msg) const &
        {
            Context copy(*this);
            copy.chain_.insert(copy.chain_.begin(), std::string(std::forward<M>(msg)));
            return copy;
        }

        /// @overload
        template <typename M>
            requires std::constructible_from<std::string, M &&>
        [[nodiscard]] Context context(M &&msg) &&
        {
            chain_.insert(chain_.begin(), std::string(std::forward<M>(msg)));
            return std::move(*this);
        }

        /**
         * @brief Appends one outer context layer produced lazily by @p f.
         *
         * @tparam F nullary callable returning a message constructible into `std::string`
         * @param f context producer, invoked exactly once
         * @return a copy of `*this` with `f()` attached as the outermost layer
         */
        template <typename F>
            requires std::copy_constructible<E> && std::invocable<F> &&
                     std::constructible_from<std::string, std::invoke_result_t<F>>
        [[nodiscard]] Context with_context(F &&f) const &
        {
            return context(std::invoke(std::forward<F>(f)));
        }

        /// @overload
        template <typename F>
            requires std::invocable<F> &&
                     std::constructible_from<std::string, std::invoke_result_t<F>>
        [[nodiscard]] Context with_context(F &&f) &&
        {
            return std::move(*this).context(std::invoke(std::forward<F>(f)));
        }

        /// @brief The original (bottom) error.
        [[nodiscard]] const E &root_cause() const & noexcept { return cause_; }
        /// @brief The original (bottom) error, moved out.
        [[nodiscard]] E &&root_cause() && noexcept { return std::move(cause_); }

        /// @brief The full context chain, ordered outermost-first.
        [[nodiscard]] const std::vector<std::string> &messages() const & noexcept { return chain_; }
        /// @brief The full context chain (moved out), ordered outermost-first.
        [[nodiscard]] std::vector<std::string> messages() && { return std::move(chain_); }

        /**
         * @brief Outermost context message as a view (empty when no layer was attached).
         *
         * @note This is only the outermost layer, not the whole chain. Use
         *       `pjh::result::render(*this)` for the full `"outer: inner: root"` text.
         *       Never bind the returned view to a temporary `Context`.
         * @note Enabled only when `E::message()` is convertible to `std::string_view`,
         *       so `Context<E>` structurally satisfies `Diagnostic` exactly when `E` does.
         */
        [[nodiscard]] std::string_view message() const noexcept
            requires requires(const E &e) {
                { e.message() } -> std::convertible_to<std::string_view>;
            }
        {
            return chain_.empty() ? std::string_view{} : std::string_view(chain_.front());
        }

        /// @brief Forwards the root cause's stable kind tag.
        /// @note Enabled only when `E::kind()` exists; comparability is required by
        ///       the `Diagnostic` concept, not by this member.
        [[nodiscard]] decltype(auto) kind() const
            requires requires(const E &e) { e.kind(); }
        {
            return root_cause().kind();
        }

        /// @brief Iterator to the first (outermost) message.
        [[nodiscard]] auto begin() const noexcept { return chain_.begin(); }
        /// @brief Iterator past the last (innermost) message.
        [[nodiscard]] auto end() const noexcept { return chain_.end(); }

    private:
        E cause_;
        std::vector<std::string> chain_;
    };
}

#endif // INCLUDE_PJH_RESULT_CONTEXT_HPP
