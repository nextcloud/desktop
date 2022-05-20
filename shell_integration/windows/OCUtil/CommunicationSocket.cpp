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

#include "CommunicationSocket.h"
#include "Log.h"
#include "StringUtil.h"
#include "UtilConstants.h"

#include <iostream>
#include <vector>
#include <array>

#include <fstream> 

#define DEFAULT_BUFLEN 4096

using namespace std;

namespace {

constexpr DWORD timeoutC = 100;

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
    pipename += L"ownCloud-";
    pipename += getUserName();
    return pipename;
}

CommunicationSocket::CommunicationSocket()
    : _pipe(INVALID_HANDLE_VALUE)
{
    _overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
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
    _pipe = CreateFile(pipename.data(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

    if (_pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    return true;
}

bool CommunicationSocket::SendMsg(const wstring &message) const
{
    auto utf8_msg = StringUtil::toUtf8(message.data(), message.size());

    DWORD numBytesWritten = 0;

    if (!WriteFile(_pipe, utf8_msg.c_str(), static_cast<DWORD>(utf8_msg.size()), &numBytesWritten, &_overlapped)) {
        if (GetLastError() == ERROR_IO_PENDING) {
            if (WaitForSingleObject(_overlapped.hEvent, timeoutC) != WAIT_OBJECT_0) {
                OCShell::logWinError(L"SendMsg timed out");
                return false;
            }
            if (!GetOverlappedResult(_pipe, &_overlapped, &numBytesWritten, FALSE)) {
                OCShell::logWinError(L"GetOverlappedResult failed");
                return false;
            }
        }
    }
    return true;
}

bool CommunicationSocket::ReadLine(wstring *response) const
{
    assert(response);
    response->clear();

    if (_pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    while (true) {
        int lbPos = 0;
        auto it = std::find(_buffer.begin() + lbPos, _buffer.end(), '\n');
        if (it != _buffer.end()) {
            *response = StringUtil::toUtf16(_buffer.data(), distance(_buffer.begin(), it));
            _buffer.erase(_buffer.begin(), it + 1);
            return true;
        }

        std::array<char, 128> resp_utf8;
        DWORD numBytesRead = 0;
        DWORD totalBytesAvailable = 0;

        if (!PeekNamedPipe(_pipe, NULL, 0, 0, &totalBytesAvailable, 0)) {
            return false;
        }
        if (totalBytesAvailable == 0) {
            return true;
        }

        if (!ReadFile(_pipe, resp_utf8.data(), DWORD(resp_utf8.size()), &numBytesRead, &_overlapped)) {
            if (GetLastError() == ERROR_IO_PENDING) {
                if (WaitForSingleObject(_overlapped.hEvent, timeoutC) != WAIT_OBJECT_0) {
                    OCShell::logWinError(L"ReadLine timed out");
                }
                if (!GetOverlappedResult(_pipe, &_overlapped, &numBytesRead, FALSE)) {
                    OCShell::logWinError(L"GetOverlappedResult failed");
                    return false;
                }
            } else {
                return false;
            }
        }
        if (numBytesRead <= 0) {
            return false;
        }
        _buffer.insert(_buffer.end(), resp_utf8.begin(), resp_utf8.begin()+numBytesRead);
        continue;
    }
}
