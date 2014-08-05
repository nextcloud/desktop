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

#ifndef PATHCHECKER_H
#define PATHCHECKER_H

#include <string>
#include <vector>

#pragma once    

class __declspec(dllexport) RemotePathChecker {
public:
	enum FileState {
		// Order synced with OCOverlay
		StateError = 0, StateErrorSWM,
		StateOk, StateOkSWM,
		StateSync, StateSyncSWM,
		StateWarning, StateWarningSWM,
		StateNone
	};
	RemotePathChecker(int port);
	std::vector<std::wstring> WatchedDirectories();
	bool IsMonitoredPath(const wchar_t* filePath, int* state);

private:
	int _StrToFileState(const std::wstring &str);
	int _port;

};

#endif