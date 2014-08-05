/**
* Copyright (c) 2014 ownCloud, Inc. All rights reserved.
*
* This library is free software; you can redistribute it and/or modify it under
* the terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 2.1 of the License
*
* This library is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
* details.
*/

#include "CommunicationSocket.h"

#include "RemotePathChecker.h"
#include "StringUtil.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iterator>

using namespace std;

RemotePathChecker::RemotePathChecker(int port)
	: _port(port)
{
}

vector<wstring> RemotePathChecker::WatchedDirectories()
{
	vector<wstring> watchedDirectories;
	wstring response;
	bool needed = false;

	CommunicationSocket socket(_port);
	socket.Connect();

	while (socket.ReadLine(&response)) {
		if (StringUtil::begins_with(response, wstring(L"REGISTER_PATH:"))) {
			size_t pathBegin = response.find(L':', 0);
			if (pathBegin == -1) {
				continue;
			}

			// chop trailing '\n'
			wstring responsePath = response.substr(pathBegin + 1, response.length()-1);
			watchedDirectories.push_back(responsePath);
		}
	}

	return watchedDirectories;
}

bool RemotePathChecker::IsMonitoredPath(const wchar_t* filePath, int* state)
{
	wstring request;
	wstring response;
	bool needed = false;

	CommunicationSocket socket(_port);
	socket.Connect();
	request = L"RETRIEVE_FILE_STATUS:";
	request += filePath;
	request += L'\n';

	if (!socket.SendMsg(request.c_str())) {
		return false;
	}

	while (socket.ReadLine(&response)) {
		// discard broadcast messages
		if (StringUtil::begins_with(response, wstring(L"STATUS:"))) {
			break;
		}
	}

	size_t statusBegin = response.find(L':', 0);
	if (statusBegin == -1)
		return false;

	size_t statusEnd = response.find(L':', statusBegin + 1);
	if (statusEnd == -1)
		return false;


	wstring responseStatus = response.substr(statusBegin+1, statusEnd - statusBegin-1);
	wstring responsePath = response.substr(statusEnd+1);
	if (responsePath == filePath) {
		if (!state) {
			return false;
		}
		*state = _StrToFileState(responseStatus);
		if (*state == StateNone) {
			return false;
		}
		needed = true;
	}

	return needed;
}

int RemotePathChecker::_StrToFileState(const std::wstring &str)
{
	if (str == L"NOP" || str == L"NONE") {
		return StateNone;
	} else if (str == L"SYNC" || str == L"NEW") {
		return StateSync;
	} else if (str == L"SYNC+SWM" || str == L"NEW+SWM") {
		return StateSyncSWM;
	} else if (str == L"OK") {
		return StateOk;
	} else if (str == L"OK+SWM") {
		return StateOkSWM;
	} else if (str == L"IGNORE") {
		return StateWarning;
	} else if (str == L"IGNORE+SWM") {
		return StateWarningSWM;
	} else if (str == L"ERROR") {
		return StateError;
	} else if (str == L"ERROR+SWM") {
		return StateErrorSWM;
	}

	return StateNone;
}