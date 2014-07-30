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

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iterator>
#include <vector>

using namespace std;

namespace {
	template<class TContainer>
	bool begins_with(const TContainer& input, const TContainer& match)
	{
		return input.size() >= match.size()
			&& equal(match.begin(), match.end(), input.begin());
	}
}

RemotePathChecker::RemotePathChecker(int port)
	: _port(port)
{
}

bool RemotePathChecker::IsMonitoredPath(const wchar_t* filePath, bool isDir)
{
	wstring request;
	wstring response;
	bool needed = false;

	CommunicationSocket socket(_port);
	socket.Connect();
	if (isDir) {
		request = L"RETRIEVE_FOLDER_STATUS:";
	} else {
		request = L"RETRIEVE_FILE_STATUS:";
	}
	request += filePath;
	request += L'\n';

	if (!socket.SendMsg(request.c_str())) {
		return false;
	}

	while ((socket.ReadLine(&response))) {
		// discard broadcast messages
		if (begins_with(response, wstring(L"STATUS:"))) {
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
		int status = _StrToFileState(responseStatus);
		if (status == StateNone) {
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