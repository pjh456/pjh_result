// custom_error.cpp — using enum and struct error types with Result.
#include <iostream>
#include <string>

#include "pjh_result/result.hpp"

namespace res = pjh::result;

// An enum error type.
enum class FileError
{
    NotFound,
    PermissionDenied,
};

// A richer struct error type.
struct ParseError
{
    std::string message;
    int position;
};

static const char *to_string(FileError e)
{
    switch (e)
    {
    case FileError::NotFound:
        return "NotFound";
    case FileError::PermissionDenied:
        return "PermissionDenied";
    }
    return "Unknown";
}

static res::Result<int, FileError> open_file(bool exists)
{
    if (!exists)
        return res::Result<int, FileError>::Err(FileError::NotFound);
    return res::Result<int, FileError>::Ok(3); // pretend fd
}

static res::Result<int, ParseError> parse(const std::string &s)
{
    if (s.empty())
        return res::Result<int, ParseError>::Err(ParseError{"empty input", 0});
    return res::Result<int, ParseError>::Ok(static_cast<int>(s.size()));
}

int main()
{
    auto f = open_file(false);
    std::cout << "open_file(false) -> " << (f.is_ok() ? "ok" : to_string(f.unwrap_err())) << '\n';

    auto p = parse("");
    if (p.is_err())
    {
        const ParseError &e = p.unwrap_err();
        std::cout << "parse(\"\") -> " << e.message << " @ " << e.position << '\n';
    }

    return 0;
}
