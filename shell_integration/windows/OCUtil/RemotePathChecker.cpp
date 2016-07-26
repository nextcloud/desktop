/**
* Copyright (c) 2014 ownCloud GmbH. All rights reserved.
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

#include "stdafx.h"

#include "CommunicationSocket.h"

#include "RemotePathChecker.h"
#include "StringUtil.h"

#include <shlobj.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iterator>
#include <unordered_set>
#include <cassert>

#include <shlobj.h>

using namespace std;

// This code is run in a thread
void RemotePathChecker::workerThreadLoop()
{
    auto pipename = CommunicationSocket::DefaultPipePath();
    bool connected = false;
    CommunicationSocket socket;
    std::unordered_set<std::wstring> asked;

    while(!_stop) {
		Sleep(50);

        if (!connected) {
            asked.clear();
            if (!WaitNamedPipe(pipename.data(), 100)) {
                continue;
            }
            if (!socket.Connect(pipename)) {
                continue;
            }
            connected = true;
            std::unique_lock<std::mutex> lock(_mutex);
            _connected = true;
        }

        {
            std::unique_lock<std::mutex> lock(_mutex);
            while (!_pending.empty() && !_stop) {
                auto filePath = _pending.front();
                _pending.pop();

                lock.unlock();
                if (!asked.count(filePath)) {
                    asked.insert(filePath);
                    socket.SendMsg(wstring(L"RETRIEVE_FILE_STATUS:" + filePath + L'\n').data());
                }
                lock.lock();
            }
        }

        std::wstring response;
        while (!_stop && socket.ReadLine(&response)) {
			if (StringUtil::begins_with(response, wstring(L"REGISTER_PATH:"))) {
				wstring responsePath = response.substr(14); // length of REGISTER_PATH:

				{   std::unique_lock<std::mutex> lock(_mutex);
					_watchedDirectories.push_back(responsePath);
				}
				SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH | SHCNF_FLUSHNOWAIT, responsePath.data(), NULL);
			} else if (StringUtil::begins_with(response, wstring(L"UNREGISTER_PATH:"))) {
                wstring responsePath = response.substr(16); // length of UNREGISTER_PATH:

                {   std::unique_lock<std::mutex> lock(_mutex);
                    _watchedDirectories.erase(
                        std::remove(_watchedDirectories.begin(), _watchedDirectories.end(), responsePath),
                        _watchedDirectories.end());

                    // Remove any item from the cache
                    for (auto it = _cache.begin(); it != _cache.end() ; ) {
                        if (StringUtil::begins_with(it->first, responsePath)) {
                            it = _cache.erase(it);
                        } else {
                            ++it;
                        }
                    }
                    // Assume that we won't need this at this point, UNREGISTER_PATH is rare
                    _oldCache.clear();
                }
				SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH | SHCNF_FLUSHNOWAIT, responsePath.data(), NULL);
            } else if (StringUtil::begins_with(response, wstring(L"STATUS:")) ||
                    StringUtil::begins_with(response, wstring(L"BROADCAST:"))) {

                auto statusBegin = response.find(L':', 0);
                assert(statusBegin != std::wstring::npos);

                auto statusEnd = response.find(L':', statusBegin + 1);
                if (statusEnd == std::wstring::npos) {
                    // the command do not contains two colon?
                    continue;
                }

                auto responseStatus = response.substr(statusBegin+1, statusEnd - statusBegin-1);
                auto responsePath = response.substr(statusEnd+1);
                auto state = _StrToFileState(responseStatus);
                bool wasAsked = asked.erase(responsePath) > 0;

                bool changed = false;
                {   std::unique_lock<std::mutex> lock(_mutex);
                    bool wasCached = _cache.find(responsePath) != _cache.end();
                    if (wasAsked || wasCached) {
                        auto &it = _cache[responsePath];
                        changed = (it != state);
                        it = state;
                    }
                }
                if (changed) {
                    SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH | SHCNF_FLUSHNOWAIT, responsePath.data(), NULL);
                }
			}
			else if (StringUtil::begins_with(response, wstring(L"UPDATE_VIEW"))) {
				std::unique_lock<std::mutex> lock(_mutex);
                // Keep the old states to continue having something to display while the new state is
                // requested from the client, triggered by clearing _cache.
                _oldCache.insert(_cache.cbegin(), _cache.cend());

                // Swap to make a copy of the cache under the mutex and clear the one stored.
                std::unordered_map<std::wstring, FileState> cache;
                swap(cache, _cache);
                lock.unlock();
                // Let explorer know about the invalidated cache entries, it will re-request the ones it needs.
                for (auto it = cache.begin(); it != cache.end(); ++it) {
                    SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH | SHCNF_FLUSHNOWAIT, it->first.data(), NULL);
                }
            }
		}

		if (socket.Event() == INVALID_HANDLE_VALUE) {
			std::unique_lock<std::mutex> lock(_mutex);
			_cache.clear();
            _oldCache.clear();
			_watchedDirectories.clear();
			_connected = connected = false;
		}

		if (_stop) return;

		HANDLE handles[2] = { _newQueries, socket.Event() };
		WaitForMultipleObjects(2, handles, false, 0);
    }
}



RemotePathChecker::RemotePathChecker()
    : _connected(false)
    , _newQueries(CreateEvent(NULL, true, true, NULL))
	, _thread([this]{ this->workerThreadLoop(); })
{
}

RemotePathChecker::~RemotePathChecker()
{
    _stop = true;
    //_newQueries.notify_all();
    SetEvent(_newQueries);
    _thread.join();
    CloseHandle(_newQueries);
}

vector<wstring> RemotePathChecker::WatchedDirectories()
{
    std::unique_lock<std::mutex> lock(_mutex);
    return _watchedDirectories;
}

bool RemotePathChecker::IsMonitoredPath(const wchar_t* filePath, int* state)
{
    assert(state); assert(filePath);

    std::unique_lock<std::mutex> lock(_mutex);
    if (!_connected) {
        return false;
    }

    auto path = std::wstring(filePath);

    auto it = _cache.find(path);
    if (it != _cache.end()) {
        *state = it->second;
        return true;
    }

    // Re-request the status while we display what we have in _oldCache
    _pending.push(filePath);

    it = _oldCache.find(path);
    bool foundInOldCache = it != _oldCache.end();
    if (foundInOldCache)
        *state = it->second;

    lock.unlock();
    SetEvent(_newQueries);
    return foundInOldCache;
}

RemotePathChecker::FileState RemotePathChecker::_StrToFileState(const std::wstring &str)
{
	if (str == L"NOP" || str == L"NONE") {
		return StateNone;
	} else if (str == L"SYNC" || str == L"NEW") {
		return StateSync;
	} else if (str == L"SYNC+SWM" || str == L"NEW+SWM") {
		return StateSync;
	} else if (str == L"OK") {
		return StateOk;
	} else if (str == L"OK+SWM") {
		return StateOkSWM;
	} else if (str == L"IGNORE") {
		return StateWarning;
	} else if (str == L"IGNORE+SWM") {
		return StateWarning;
	} else if (str == L"ERROR") {
		return StateError;
	} else if (str == L"ERROR+SWM") {
		return StateError;
	}

	return StateNone;
}
