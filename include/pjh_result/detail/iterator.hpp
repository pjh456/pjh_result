/**
 * @file iterator.hpp
 * @brief Zero-or-one proxy iterator shared by `Option` and `Result`.
 */
#ifndef INCLUDE_PJH_RESULT_DETAIL_ITERATOR_HPP
#define INCLUDE_PJH_RESULT_DETAIL_ITERATOR_HPP

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace pjh::result::detail
{
    /**
     * @brief Proxy iterator over zero or one element of stored type @p S.
     *
     * Models the Rust `Option::Iter` / `Result::Iter` shape: an empty or
     * single-element sequence. The iterator is also its own range, so
     * `for (auto &x : opt.iter())` and `std::ranges::find(opt.iter(), v)` work.
     *
     * @tparam S element type stored by the owning container (`Unit` for the
     *         valueless `T = void` cases; the public methods are disabled there)
     * @tparam IsConst `true` for the immutable (`iter()`) variant
     *
     * @warning Borrows a single element owned by the container; it must not
     *          outlive the container nor be used after the container is
     *          reassigned, moved, or `take()`n from.
     */
    template <typename S, bool IsConst>
    class SingleIter
    {
    public:
        using value_type = S;
        using reference = std::conditional_t<IsConst, const S &, S &>;
        using pointer = std::conditional_t<IsConst, const S *, S *>;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;

        SingleIter() noexcept = default;
        explicit SingleIter(pointer p) noexcept : ptr_(p) {}

        reference operator*() const noexcept { return *ptr_; }
        pointer operator->() const noexcept { return ptr_; }

        SingleIter &operator++() noexcept
        {
            ptr_ = nullptr; // single-element: one increment reaches end
            return *this;
        }
        SingleIter operator++(int) noexcept
        {
            SingleIter tmp = *this;
            ++(*this);
            return tmp;
        }

        /// @brief The iterator is its own range: current position.
        SingleIter begin() const noexcept { return *this; }
        /// @brief One-past-the-end (empty) position.
        SingleIter end() const noexcept { return SingleIter{}; }

        friend bool operator==(const SingleIter &a, const SingleIter &b) noexcept
        {
            return a.ptr_ == b.ptr_;
        }

    private:
        pointer ptr_ = nullptr;
    };
}

#endif // INCLUDE_PJH_RESULT_DETAIL_ITERATOR_HPP
