/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-FileCopyrightText: 2000-2013 Liferay, Inc. All rights reserved
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "CommunicationSocket.h"
#include "StringUtil.h"
#include "WinShellExtConstants.h"

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
    auto pipename = std::wstring(LR"(\\.\pipe\)");
    pipename += std::wstring(UTIL_PIPE_APP_NAME);
    pipename += L"-";
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
    _pipe = CreateFile(pipename.data(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

    if (_pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    return true;
}

bool CommunicationSocket::SendMsg(const wchar_t* message, std::ofstream &logger) const
{
    logger << "CommunicationSocket::SendMsg: " << (*message) << std::endl;

    auto utf8_msg = StringUtil::toUtf8(message);

    DWORD numBytesWritten = 0;

    if (!WriteFile(_pipe, utf8_msg.c_str(), static_cast<DWORD>(utf8_msg.size()), &numBytesWritten, &_overlapped)) {
        if (GetLastError() == ERROR_IO_PENDING) {

            if (WaitForSingleObject(_overlapped.hEvent, timeoutC) != WAIT_OBJECT_0) {
                logger << "SendMsg timed out" << std::endl;
                return false;
            }

            if (!GetOverlappedResult(_pipe, &_overlapped, &numBytesWritten, FALSE)) {
                logger << "GetOverlappedResult failed" << std::endl;
                return false;
            }

        }
    }

    return true;
}

bool CommunicationSocket::ReadLine(wstring* response, std::ofstream &logger) const
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
            *response = StringUtil::toUtf16(_buffer.data(), DWORD(it - _buffer.begin()));
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
                    logger << "ReadLine timed out" << std::endl;
                }
                if (!GetOverlappedResult(_pipe, &_overlapped, &numBytesRead, FALSE)) {
                    logger << "GetOverlappedResult failedt" << std::endl;
                    return false;
                }
            } else {
                return false;
            }
        }
        if (numBytesRead <= 0) {
            return false;
        }
        _buffer.insert(_buffer.end(), resp_utf8.begin(), resp_utf8.begin() + numBytesRead);
        continue;
    }
}
