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

#include "stdafx.h"

#include "CommunicationSocket.h"
#include "UtilConstants.h"
#include "StringUtil.h"

#include "config.h"

#include <iostream>
#include <vector>
#include <array>

#include <fstream> 

#define DEFAULT_BUFLEN 4096

using namespace std;

namespace {

std::wstring getUserName() {
    DWORD  len = DEFAULT_BUFLEN;
    TCHAR  buf[DEFAULT_BUFLEN];
    if (GetUserName(buf, &len)) {
        return std::wstring(&buf[0], len);
    } else {
        return std::wstring();
    }
}

}

std::wstring CommunicationSocket::DefaultPipePath()
{
    auto pipename = std::wstring(L"\\\\.\\pipe\\");
    pipename += APPLICATION_SHORTNAME;
    pipename += L"-"
    pipename += getUserName();
    return pipename;
}

CommunicationSocket::CommunicationSocket()
    : _pipe(INVALID_HANDLE_VALUE)
{
}

CommunicationSocket::~CommunicationSocket()
{
    Close();
}

bool CommunicationSocket::Close()
{
    if (_pipe == INVALID_HANDLE_VALUE) {
        return false;
    }
    CloseHandle(_pipe);
    _pipe = INVALID_HANDLE_VALUE;
    return true;
}


bool CommunicationSocket::Connect(const std::wstring &pipename)
{
    _pipe = CreateFile(pipename.data(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (_pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    return true;
}

bool CommunicationSocket::SendMsg(const wchar_t* message) const
{
    auto utf8_msg = StringUtil::toUtf8(message);

    DWORD numBytesWritten = 0;
    auto result = WriteFile( _pipe, utf8_msg.c_str(), DWORD(utf8_msg.size()), &numBytesWritten, NULL);

    if (result) {
        return true;
    } else {
        const_cast<CommunicationSocket*>(this)->Close();

        return false;
    }
}

bool CommunicationSocket::ReadLine(wstring* response)
{
    if (!response) {
        return false;
    }

    response->clear();

    if (_pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    while (true) {
        int lbPos = 0;
        auto it = std::find(_buffer.begin() + lbPos, _buffer.end(), '\n');
        if (it != _buffer.end()) {
            *response = StringUtil::toUtf16(_buffer.data(), DWORD(it - _buffer.begin()));
            _buffer.erase(_buffer.begin(), it + 1);
            return true;
        }

        std::array<char, 128> resp_utf8;
        DWORD numBytesRead = 0;
        DWORD totalBytesAvailable = 0;

        if (!PeekNamedPipe(_pipe, NULL, 0, 0, &totalBytesAvailable, 0)) {
            Close();
            return false;
        }
        if (totalBytesAvailable == 0) {
            return false;
        }

        if (!ReadFile(_pipe, resp_utf8.data(), DWORD(resp_utf8.size()), &numBytesRead, NULL)) {
            Close();
            return false;
        }
        if (numBytesRead <= 0) {
            return false;
        }
        _buffer.insert(_buffer.end(), resp_utf8.begin(), resp_utf8.begin()+numBytesRead);
        continue;
    }
}
