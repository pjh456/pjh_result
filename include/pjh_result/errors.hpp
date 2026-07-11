/**
 * @file errors.hpp
 * @brief Exception type thrown on invalid `Result` / `Option` access.
 */
#ifndef INCLUDE_PJH_RESULT_ERRORS_HPP
#define INCLUDE_PJH_RESULT_ERRORS_HPP

#include <stdexcept>
#include <string>

namespace pjh::result
{
    /// @brief Thrown when unwrapping in the wrong state (e.g. `unwrap()` on Err / None).
    class bad_result_access : public std::logic_error
    {
    public:
        /// @brief Constructs the exception with a human-readable message.
        /// @param msg description of the invalid access
        explicit bad_result_access(
            const std::string &msg)
            : std::logic_error(msg) {}
    };
}

#endif // INCLUDE_PJH_RESULT_ERRORS_HPP
