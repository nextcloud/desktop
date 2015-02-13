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

#include "stdafx.h"

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

using namespace std;

#define PIPE_TIMEOUT  5*1000 //ms
#define SOCK_BUFFER 4096

OCClientInterface::ContextMenuInfo OCClientInterface::FetchInfo()
{
	auto pipename = std::wstring(L"\\\\.\\pipe\\");
	pipename += L"ownCloud";

	CommunicationSocket socket;
	if (!WaitNamedPipe(pipename.data(), PIPE_TIMEOUT)) {
		return {};
	}
	if (!socket.Connect(pipename)) {
		return {};
	}
	socket.SendMsg(L"SHARE_MENU_TITLE\n");

	ContextMenuInfo info;
	std::wstring response;
	int sleptCount = 0;
	while (sleptCount < 5) {
		if (socket.ReadLine(&response)) {
			if (StringUtil::begins_with(response, wstring(L"REGISTER_PATH:"))) {
				wstring responsePath = response.substr(14); // length of REGISTER_PATH
				info.watchedDirectories.push_back(responsePath);
			}
			else if (StringUtil::begins_with(response, wstring(L"SHARE_MENU_TITLE:"))) {
				info.shareMenuTitle = response.substr(17); // length of SHARE_MENU_TITLE:
				break; // Stop once we received the last sent request
			}
		}
		else {
			Sleep(50);
			++sleptCount;
		}
	}
	return info;
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
