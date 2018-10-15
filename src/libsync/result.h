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

namespace OCC {

/**
 * A Result of type T, or an Error that contains a code and a string
 *
 * The code is an HTTP error code.
 **/
template <typename T>
class Result
{
    struct Error
    {
        QString string;
        int code;
    };
    union {
        T _result;
        Error _error;
    };
    bool _isError;

public:
    Result(T value)
        : _result(std::move(value))
        , _isError(false){};
    Result(int code, QString str)
        : _error({ std::move(str), code })
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
    QString errorMessage() const
    {
        ASSERT(_isError);
        return _error.string;
    }
    int errorCode() const
    {
        return _isError ? _error.code : 0;
    }
};

} // namespace OCC
