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
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

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
	RemotePathChecker();
    ~RemotePathChecker();
	std::vector<std::wstring> WatchedDirectories();
	bool IsMonitoredPath(const wchar_t* filePath, int* state);

private:
	FileState _StrToFileState(const std::wstring &str);
    std::mutex _mutex;
    std::atomic<bool> _stop;

    // Everything here is protected by the _mutex

    /** The list of paths we need to query. The main thread fill this, and the worker thread
     * send that to the socket. */
    std::queue<std::wstring> _pending;

    std::unordered_map<std::wstring, FileState> _cache;
    std::vector<std::wstring> _watchedDirectories;
    bool _connected;


    // The main thread notifies when there are new items in _pending
    //std::condition_variable _newQueries;
    HANDLE _newQueries;

	std::thread _thread;
    void workerThreadLoop();
};

#endif