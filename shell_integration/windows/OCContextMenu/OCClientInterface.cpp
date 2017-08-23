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
    auto pipename = CommunicationSocket::DefaultPipePath();

    CommunicationSocket socket;
    if (!WaitNamedPipe(pipename.data(), PIPE_TIMEOUT)) {
        return {};
    }
    if (!socket.Connect(pipename)) {
        return {};
    }
    socket.SendMsg(L"GET_STRINGS\n");

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
                if (stringName == L"SHARE_MENU_TITLE")
                    info.shareMenuTitle = move(stringValue);
                else if (stringName == L"CONTEXT_MENU_TITLE")
                    info.contextMenuTitle = move(stringValue);
                else if (stringName == L"COPY_PRIVATE_LINK_MENU_TITLE")
                    info.copyLinkMenuTitle = move(stringValue);
                else if (stringName == L"EMAIL_PRIVATE_LINK_MENU_TITLE")
                    info.emailLinkMenuTitle = move(stringValue);
            }
            else if (StringUtil::begins_with(response, wstring(L"GET_STRINGS:END"))) {
                break; // Stop once we completely received the last sent request
            }
        }
        else {
            Sleep(50);
            ++sleptCount;
        }
    }
    return info;
}

void OCClientInterface::RequestShare(const std::wstring &path)
{
    SendRequest(L"SHARE", path);
}

void OCClientInterface::RequestCopyLink(const std::wstring &path)
{
    SendRequest(L"COPY_PRIVATE_LINK", path);
}

void OCClientInterface::RequestEmailLink(const std::wstring &path)
{
    SendRequest(L"EMAIL_PRIVATE_LINK", path);
}

void OCClientInterface::SendRequest(wchar_t *verb, const std::wstring &path)
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
