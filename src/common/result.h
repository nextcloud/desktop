/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "asserts.h"

namespace OCC {

/**
 * A Result of type T, or an Error
 */
template <typename T, typename Error>
class Result
{
    union {
        T _result;
        Error _error;
    };
    bool _isError;

public:
    Result(T value)
        : _result(std::move(value))
        , _isError(false)
    {
    }
    // TODO: This doesn't work if T and Error are too similar
    Result(Error error)
        : _error(std::move(error))
        , _isError(true)
    {
    }

    Result(Result &&other)
        : _isError(other._isError)
    {
        if (_isError) {
            new (&_error) Error(std::move(other._error));
        } else {
            new (&_result) T(std::move(other._result));
        }
    }

    Result(const Result &other)
        : _isError(other._isError)
    {
        if (_isError) {
            new (&_error) Error(other._error);
        } else {
            new (&_result) T(other._result);
        }
    }

    Result &operator=(Result &&other)
    {
        if (&other != this) {
            // Destroy the currently held value before constructing the new one
            if (_isError) {
                _error.~Error();
            } else {
                _result.~T();
            }
            _isError = other._isError;
            if (_isError) {
                new (&_error) Error(std::move(other._error));
            } else {
                new (&_result) T(std::move(other._result));
            }
        }
        return *this;
    }

    Result &operator=(const Result &other)
    {
        if (&other != this) {
            // Destroy the currently held value before constructing the new one
            if (_isError) {
                _error.~Error();
            } else {
                _result.~T();
            }
            _isError = other._isError;
            if (_isError) {
                new (&_error) Error(other._error);
            } else {
                new (&_result) T(other._result);
            }
        }
        return *this;
    }

    ~Result()
    {
        if (_isError)
            _error.~Error();
        else
            _result.~T();
    }

    explicit operator bool() const { return !_isError; }

    const T &operator*() const &
    {
        ASSERT(!_isError);
        return _result;
    }

    T operator*() &&
    {
        ASSERT(!_isError);
        return std::move(_result);
    }

    const T *operator->() const
    {
        ASSERT(!_isError);
        return &_result;
    }

    [[nodiscard]] const T &get() const
    {
        ASSERT(!_isError)
        return _result;
    }

    [[nodiscard]] const Error &error() const &
    {
        ASSERT(_isError);
        return _error;
    }
    Error error() &&
    {
        ASSERT(_isError);
        return std::move(_error);
    }

    [[nodiscard]] bool isValid() const { return !_isError; }
};

namespace detail {
    struct NoResultData{};
}

template <typename Error>
class Result<void, Error> : public Result<detail::NoResultData, Error>
{
public:
    using Result<detail::NoResultData, Error>::Result;
    Result() : Result(detail::NoResultData{}) {}
};

namespace detail {
struct OptionalNoErrorData{};
}

template <typename T>
class Optional : public Result<T, detail::OptionalNoErrorData>
{
public:
    using Result<T, detail::OptionalNoErrorData>::Result;

    Optional()
        : Optional(detail::OptionalNoErrorData{})
    {
    }
};

} // namespace OCC
