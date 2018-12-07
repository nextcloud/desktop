/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
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
    const Error &error() const &
    {
        ASSERT(_isError);
        return _error;
    }
    Error error() &&
    {
        ASSERT(_isError);
        return std::move(_error);
    }
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
