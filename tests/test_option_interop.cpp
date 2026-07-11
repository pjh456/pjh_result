#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "pjh_result/option.hpp"

namespace res = pjh::result;

TEST_CASE("ok_or converts Some to Ok, None to Err")
{
    auto s = res::Option<int>::Some(7);
    auto r = s.ok_or(std::string("missing"));
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 7);

    auto n = res::Option<int>::None();
    auto e = n.ok_or(std::string("missing"));
    CHECK(e.is_err());
    CHECK(e.unwrap_err() == "missing");
}

TEST_CASE("ok_or_else lazily computes the error")
{
    auto s = res::Option<int>::Some(7);
    auto r = s.ok_or_else(
        []()
        { return std::string("missing"); });
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 7);

    auto n = res::Option<int>::None();
    auto e = n.ok_or_else(
        []()
        { return std::string("computed"); });
    CHECK(e.is_err());
    CHECK(e.unwrap_err() == "computed");
}

TEST_CASE("ok_or works with void Option")
{
    auto s = res::Option<void>::Some();
    auto r = s.ok_or(std::string("missing"));
    CHECK(r.is_ok());
    r.unwrap(); // returns void, must not throw

    auto n = res::Option<void>::None();
    auto e = n.ok_or(std::string("missing"));
    CHECK(e.is_err());
    CHECK(e.unwrap_err() == "missing");
}

TEST_CASE("ok_or works with move-only error type")
{
    auto s = res::Option<int>::Some(7);
    auto r = s.ok_or(std::unique_ptr<int>(new int(42)));
    CHECK(r.is_ok());
    CHECK(r.unwrap() == 7);

    auto n = res::Option<int>::None();
    auto e = n.ok_or(std::unique_ptr<int>(new int(99)));
    CHECK(e.is_err());
    CHECK(*e.unwrap_err() == 99);
}

TEST_CASE("transpose Some(Ok) becomes Ok(Some)")
{
    auto inner = res::Result<int, std::string>::Ok(42);
    auto opt = res::Option<res::Result<int, std::string>>::Some(std::move(inner));
    auto tr = opt.transpose();
    CHECK(tr.is_ok());
    CHECK(tr.unwrap().is_some());
    CHECK(tr.unwrap().unwrap() == 42);
}

TEST_CASE("transpose Some(Err) becomes Err")
{
    auto inner = res::Result<int, std::string>::Err(std::string("e"));
    auto opt = res::Option<res::Result<int, std::string>>::Some(std::move(inner));
    auto tr = opt.transpose();
    CHECK(tr.is_err());
    CHECK(tr.unwrap_err() == "e");
}

TEST_CASE("transpose None becomes Ok(None)")
{
    auto opt = res::Option<res::Result<int, std::string>>::None();
    auto tr = opt.transpose();
    CHECK(tr.is_ok());
    CHECK(tr.unwrap().is_none());
}

TEST_CASE("transpose rvalue moves inner Result")
{
    auto inner = res::Result<std::unique_ptr<int>, std::string>::Ok(std::unique_ptr<int>(new int(7)));
    auto opt = res::Option<res::Result<std::unique_ptr<int>, std::string>>::Some(std::move(inner));
    auto tr = std::move(opt).transpose();
    CHECK(tr.is_ok());
    CHECK(*tr.unwrap().unwrap() == 7);
}

TEST_CASE("transpose void inner Ok")
{
    auto inner = res::Result<void, std::string>::Ok();
    auto opt = res::Option<res::Result<void, std::string>>::Some(std::move(inner));
    auto tr = opt.transpose();
    CHECK(tr.is_ok());
    CHECK(tr.unwrap().is_some());
}

TEST_CASE("transpose void inner Err")
{
    auto inner = res::Result<void, std::string>::Err(std::string("e"));
    auto opt = res::Option<res::Result<void, std::string>>::Some(std::move(inner));
    auto tr = opt.transpose();
    CHECK(tr.is_err());
    CHECK(tr.unwrap_err() == "e");
}
