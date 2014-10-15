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
#include "UtilConstants.h"
#include "StringUtil.h"

#include <WinSock2.h>
#include <Ws2def.h>
#include <windows.h>
#include <iostream>
#include <vector>
#include <array>

#include <fstream> 

#define BUFSIZE 1024

using namespace std;

#define DEFAULT_BUFLEN 4096

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
	WSACleanup();
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

bool CommunicationSocket::SendMsg(const wchar_t* message)
{
	auto utf8_msg = StringUtil::toUtf8(message);

    DWORD numBytesWritten = 0;
    auto result = WriteFile( _pipe, utf8_msg.c_str(), DWORD(utf8_msg.size()), &numBytesWritten, NULL);

    if (result) {
        return true;
    } else {
        Close();

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
		auto result = PeekNamedPipe(_pipe, NULL, 0, 0, &totalBytesAvailable, 0);
		if (!result) {
			Close();
			return false;
		}
		if (totalBytesAvailable == 0) {
			return false;
		}

        result = ReadFile(_pipe, resp_utf8.data(), DWORD(resp_utf8.size()), &numBytesRead, NULL);
        if (!result) {
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
