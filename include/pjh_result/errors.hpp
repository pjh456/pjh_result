#ifndef INCLUDE_PJH_RESULT_BAD_RESULT_ACCESS_HPP
#define INCLUDE_PJH_RESULT_BAD_RESULT_ACCESS_HPP

#include <stdexcept>
#include <string>

namespace pjh::result::utils::result_helper
{
    class bad_result_access : public std::logic_error
    {
    public:
        explicit bad_result_access(
            const std::string &msg)
            : std::logic_error(msg) {}
    };
}

#endif // INCLUDE_PJH_RESULT_BAD_RESULT_ACCESS_HPP
