/**
* Copyright (c) 2015 Daniel Molkentin <danimo@owncloud.com>. All rights reserved.
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



#include "OCClientInterface.h"

#include "CommunicationSocket.h"
#include "StringUtil.h"

#include <shlobj.h>

#include <Strsafe.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iterator>
#include <unordered_set>
#include <cassert>

using namespace std;

#define PIPE_TIMEOUT  5*1000 //ms
#define SOCK_BUFFER 4096

std::vector<std::wstring> OCClientInterface::WatchedDirectories()
{
	auto pipename = std::wstring(L"\\\\.\\pipe\\");
	pipename += L"ownCloud";

	CommunicationSocket socket;
	if (!WaitNamedPipe(pipename.data(), PIPE_TIMEOUT)) {
		return std::vector<std::wstring>();
	}
	if (!socket.Connect(pipename)) {
		return std::vector<std::wstring>();
	}
	std::vector<std::wstring> watchedDirectories;
	std::wstring response;
	while (socket.ReadLine(&response, true)) {
		if (StringUtil::begins_with(response, wstring(L"REGISTER_PATH:"))) {
			wstring responsePath = response.substr(14); // length of REGISTER_PATH
			watchedDirectories.push_back(responsePath);
		}
	}
	return watchedDirectories;
}

void OCClientInterface::ShareObject(const std::wstring &path)
{
	auto pipename = std::wstring(L"\\\\.\\pipe\\");
	pipename += L"ownCloud";

	CommunicationSocket socket;
	if (!WaitNamedPipe(pipename.data(), PIPE_TIMEOUT)) {
		return;
	}
	if (!socket.Connect(pipename)) {
		return;
	}

	wchar_t msg[SOCK_BUFFER] = { 0 };
	if (SUCCEEDED(StringCchPrintf(msg, SOCK_BUFFER, L"SHARE:%s\n", path.c_str())))
	{
		socket.SendMsg(msg);
	}
}
