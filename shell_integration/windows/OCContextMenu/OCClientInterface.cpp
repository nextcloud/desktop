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

OCClientInterface::ContextMenuInfo OCClientInterface::FetchInfo(const std::wstring &files)
{
    auto pipename = CommunicationSocket::DefaultPipePath();

    CommunicationSocket socket;
    if (!WaitNamedPipe(pipename.data(), PIPE_TIMEOUT)) {
        return {};
    }
    if (!socket.Connect(pipename)) {
        return {};
    }
    socket.SendMsg(L"GET_STRINGS:CONTEXT_MENU_TITLE\n");
    socket.SendMsg((L"GET_MENU_ITEMS:" + files + L"\n").data());

    ContextMenuInfo info;
    std::wstring response;
    int sleptCount = 0;
    while (sleptCount < 5) {
        if (socket.ReadLine(&response)) {
            if (StringUtil::begins_with(response, wstring(L"REGISTER_PATH:"))) {
                wstring responsePath = response.substr(14); // length of REGISTER_PATH
                info.watchedDirectories.push_back(responsePath);
            }
            else if (StringUtil::begins_with(response, wstring(L"STRING:"))) {
                wstring stringName, stringValue;
                if (!StringUtil::extractChunks(response, stringName, stringValue))
                    continue;
                if (stringName == L"CONTEXT_MENU_TITLE")
                    info.contextMenuTitle = move(stringValue);
            } else if (StringUtil::begins_with(response, wstring(L"MENU_ITEM:"))) {
                wstring commandName, flags, title;
                if (!StringUtil::extractChunks(response, commandName, flags, title))
                    continue;
                info.menuItems.push_back({ commandName, flags, title });
            } else if (StringUtil::begins_with(response, wstring(L"GET_MENU_ITEMS:END"))) {
                break; // Stop once we completely received the last sent request
            }
            else if (StringUtil::begins_with(response, wstring(L"STREAM_SUBMENU_TITLE:"))) {
                info.streamSubMenuTitle = response.substr(21);
            }
            else if (StringUtil::begins_with(response, wstring(L"STREAM_OFFLINE_ITEM_TITLE:"))) {
                info.streamOfflineItemTitle = response.substr(26);
            }
            else if (StringUtil::begins_with(response, wstring(L"STREAM_ONLINE_ITEM_TITLE:"))) {
                info.streamOnlineItemTitle = response.substr(25);
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

void OCClientInterface::SendRequest(const wchar_t *verb, const std::wstring &path)
{
    auto pipename = CommunicationSocket::DefaultPipePath();

    CommunicationSocket socket;
    if (!WaitNamedPipe(pipename.data(), PIPE_TIMEOUT)) {
        return;
    }
    if (!socket.Connect(pipename)) {
        return;
    }

    wchar_t msg[SOCK_BUFFER] = { 0 };
    if (SUCCEEDED(StringCchPrintf(msg, SOCK_BUFFER, L"%s:%s\n", verb, path.c_str())))
    {
        socket.SendMsg(msg);
    }
}

void OCClientInterface::SetDownloadMode(const std::wstring &path, bool online)
{
    auto pipename = CommunicationSocket::DefaultPipePath();

    CommunicationSocket socket;
    if (!WaitNamedPipe(pipename.data(), PIPE_TIMEOUT)) {
        return;
    }
    if (!socket.Connect(pipename)) {
        return;
    }

    wchar_t msg[SOCK_BUFFER] = { 0 };
    if (SUCCEEDED(StringCchPrintf(msg, 
            SOCK_BUFFER, 
            L"SET_DOWNLOAD_MODE:%s|%d\n", 
            path.c_str(),
            online)
            )
        )
    {
        socket.SendMsg(msg);
    }
}

std::wstring OCClientInterface::GetDownloadMode(const std::wstring &path)
{
    CommunicationSocket socket;
    auto pipename = CommunicationSocket::DefaultPipePath();

    if (!WaitNamedPipe(pipename.data(), PIPE_TIMEOUT))
        return L"";
    if (!socket.Connect(pipename))
        return L"";

    wchar_t msg[SOCK_BUFFER] = { 0 };
    if (!SUCCEEDED(
            StringCchPrintf(msg, 
                SOCK_BUFFER, 
                L"GET_DOWNLOAD_MODE:%s\n", 
                path.c_str())
            )
        )
        return L"";
    
    socket.SendMsg(msg);
    std::wstring response;

    int sleptCount = 0;
    while (sleptCount < 5)
        if (socket.ReadLine(&response))
        {
            if (StringUtil::begins_with(response, wstring(L"GET_DOWNLOAD_MODE:"))) {
                wstring downloadMode = response.substr(18);
                return downloadMode;
            }
        }
        else 
        {
            Sleep(50);
            ++sleptCount;
        }
    return L"";
}
