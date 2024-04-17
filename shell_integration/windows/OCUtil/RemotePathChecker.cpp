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
                    if (!socket.SendMsg(L"RETRIEVE_FILE_STATUS:" + filePath + L'\n')) {
                        _stop = true;
                        break;
                    }
                }
                lock.lock();
            }
        }

        std::wstring response;
        while (!_stop) {
            if (!socket.ReadLine(&response)) {
                socket.Close();
                break;
            }
            if (response.empty()) {
                break;
            }
            if (StringUtil::begins_with(response, wstring(L"REGISTER_PATH:"))) {
                wstring responsePath = response.substr(14); // length of REGISTER_PATH:

                auto sharedPtrCopy = atomic_load(&_watchedDirectories);
                auto vectorCopy = make_shared<vector<wstring>>(*sharedPtrCopy);
                vectorCopy->push_back(responsePath);
                atomic_store(&_watchedDirectories, shared_ptr<const vector<wstring>>(vectorCopy));

                // We don't keep track of all files and can't know which file is currently visible
                // to the user, but at least reload the root dir so that any shortcut to the root
                // is updated without the user needing to refresh.
                SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH | SHCNF_FLUSHNOWAIT, responsePath.data(), nullptr);
            } else if (StringUtil::begins_with(response, wstring(L"UNREGISTER_PATH:"))) {
                wstring responsePath = response.substr(16); // length of UNREGISTER_PATH:

                auto sharedPtrCopy = atomic_load(&_watchedDirectories);
                auto vectorCopy = make_shared<vector<wstring>>(*sharedPtrCopy);
                vectorCopy->erase(
                    std::remove(vectorCopy->begin(), vectorCopy->end(), responsePath),
                    vectorCopy->end());
                atomic_store(&_watchedDirectories, shared_ptr<const vector<wstring>>(vectorCopy));

                vector<wstring> removedPaths;
                {   std::unique_lock<std::mutex> lock(_mutex);
                    // Remove any item from the cache
                    for (auto it = _cache.begin(); it != _cache.end() ; ) {
                        if (StringUtil::isDescendantOf(it->first, responsePath)) {
                            removedPaths.emplace_back(std::move(it->first));
                            it = _cache.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
                for (auto& path : removedPaths)
                    SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH | SHCNF_FLUSHNOWAIT, path.data(), nullptr);
            } else if (StringUtil::begins_with(response, wstring(L"STATUS:")) ||
                    StringUtil::begins_with(response, wstring(L"BROADCAST:"))) {

                wstring responseStatus, responsePath;
                if (!StringUtil::extractChunks(response, responseStatus, responsePath))
                    continue;

                auto state = _StrToFileState(responseStatus);
                bool wasAsked = asked.erase(responsePath) > 0;

                bool updateView = false;
                {   std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _cache.find(responsePath);
                    if (it == _cache.end()) {
                        // The client only approximates requested files, if the bloom
                        // filter becomes saturated after navigating multiple directories we'll start getting
                        // status pushes that we never requested and fill our cache. Ignore those.
                        if (!wasAsked) {
                            continue;
                        }
                        it = _cache.insert(make_pair(responsePath, StateNone)).first;
                    }

                    updateView = it->second != state;
                    it->second = state;
                }
                if (updateView) {
                    SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH | SHCNF_FLUSHNOWAIT, responsePath.data(), nullptr);
                }
            }
        }

        if (socket.Event() == INVALID_HANDLE_VALUE) {
            atomic_store(&_watchedDirectories, make_shared<const vector<wstring>>());
            std::unique_lock<std::mutex> lock(_mutex);
            _connected = connected = false;

            // Swap to make a copy of the cache under the mutex and clear the one stored.
            std::unordered_map<std::wstring, FileState> cache;
            swap(cache, _cache);
            lock.unlock();
            // Let explorer know about each invalidated cache entry that needs to get its icon removed.
            for (auto it = cache.begin(); it != cache.end(); ++it) {
                SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH | SHCNF_FLUSHNOWAIT, it->first.data(), nullptr);
            }
        }

        if (_stop) {
            return;
        }

        HANDLE handles[2] = { _newQueries, socket.Event() };
        WaitForMultipleObjects(2, handles, false, 0);
    }
}


RemotePathChecker::RemotePathChecker()
    : _stop(false)
    , _watchedDirectories(make_shared<const vector<wstring>>())
    , _connected(false)
    , _newQueries(CreateEvent(nullptr, FALSE, FALSE, nullptr))
    , _thread([this] { this->workerThreadLoop(); })
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

std::shared_ptr<const std::vector<std::wstring>> RemotePathChecker::WatchedDirectories() const
{
    return atomic_load(&_watchedDirectories);
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
        // The path is in our cache, and we'll get updates pushed if the status changes.
        *state = it->second;
        return true;
    }

    _pending.push(filePath);

    lock.unlock();
    SetEvent(_newQueries);
    return false;
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
