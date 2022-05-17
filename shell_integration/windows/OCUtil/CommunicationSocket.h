/**
 * Copyright (c) 2000-2013 Liferay, Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 */

#ifndef COMMUNICATIONSOCKET_H
#define COMMUNICATIONSOCKET_H

#pragma once

#pragma warning (disable : 4251)

#include <string>
#include <vector>
#include <WinSock2.h>

class __declspec(dllexport) CommunicationSocket
{
public:
    static std::wstring DefaultPipePath();

    CommunicationSocket();
    ~CommunicationSocket();

    bool Connect(const std::wstring& pipename);
    bool Close();

    [[nodiscard]] bool SendMsg(const std::wstring &) const;
    [[nodiscard]] bool ReadLine(std::wstring *) const;

    HANDLE Event() { return _pipe; }

private:    
    HANDLE _pipe;
    mutable std::vector<char> _buffer;
    bool _connected;

    mutable OVERLAPPED _overlapped = {};
};

#endif
