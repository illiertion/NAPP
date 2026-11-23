#pragma once

#include "napp/core/error.h"

#include <utility>

namespace napp {

// Minimal expected-like type: NAPP code avoids exceptions for recoverable
// failures, so every fallible operation returns Result<T>.
template <typename T>
class Result
{
public:
    Result(T value) : m_value(std::move(value)), m_ok(true) {}
    Result(Error error) : m_error(std::move(error)), m_ok(false) {}

    static Result ok(T value) { return Result(std::move(value)); }
    static Result fail(Error error) { return Result(std::move(error)); }
    static Result fail(ErrorCode code, const QString& message, const QString& detail = {})
    {
        return Result(Error::make(code, message, detail));
    }

    bool isOk() const { return m_ok; }
    bool isError() const { return !m_ok; }
    explicit operator bool() const { return m_ok; }

    const T& value() const { return m_value; }
    T& value() { return m_value; }
    const Error& error() const { return m_error; }

private:
    T m_value{};
    Error m_error;
    bool m_ok = false;
};

template <>
class Result<void>
{
public:
    Result() : m_ok(true) {}
    Result(Error error) : m_error(std::move(error)), m_ok(false) {}

    static Result ok() { return Result(); }
    static Result fail(Error error) { return Result(std::move(error)); }
    static Result fail(ErrorCode code, const QString& message, const QString& detail = {})
    {
        return Result(Error::make(code, message, detail));
    }

    bool isOk() const { return m_ok; }
    bool isError() const { return !m_ok; }
    explicit operator bool() const { return m_ok; }
    const Error& error() const { return m_error; }

private:
    Error m_error;
    bool m_ok = false;
};

} // namespace napp
