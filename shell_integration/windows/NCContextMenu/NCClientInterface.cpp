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

#include "NCClientInterface.h"

#include "CommunicationSocket.h"
#include "StringUtil.h"

#include <shlobj.h>

#include <Strsafe.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iterator>
#include <unordered_set>
#include <locale>
#include <codecvt>

using namespace std;

#define PIPE_TIMEOUT  5*1000 //ms

NCClientInterface::ContextMenuInfo NCClientInterface::FetchInfo(const std::wstring &files, std::ofstream &logger)
{
    auto pipename = CommunicationSocket::DefaultPipePath();

    CommunicationSocket socket;
    if (!WaitNamedPipe(pipename.data(), PIPE_TIMEOUT)) {
        logger << "error with WaitNamedPipe" << std::endl;
        return {};
    }
    if (!socket.Connect(pipename)) {
        logger << "error with Connect" << std::endl;
        return {};
    }
    socket.SendMsg(L"GET_STRINGS:CONTEXT_MENU_TITLE\n", logger);
    socket.SendMsg((L"GET_MENU_ITEMS:" + files + L"\n").data(), logger);

    ContextMenuInfo info;
    std::wstring response;
    int sleptCount = 0;

    constexpr auto noReplyTimeout = 20;
    constexpr auto replyTimeout = 200;
    bool receivedReplyFromDesktopClient = false;
    while ((!receivedReplyFromDesktopClient && sleptCount < noReplyTimeout) || (receivedReplyFromDesktopClient && sleptCount < replyTimeout)) {
        logger << "trying to read a line" << std::endl;

        if (socket.ReadLine(&response)) {
            logger << "received: " << StringUtil::toUtf8(response.c_str()) << std::endl;
            if (StringUtil::begins_with(response, wstring(L"REGISTER_PATH:"))) {
                logger << "received: REGISTER_PATH" << std::endl;
                wstring responsePath = response.substr(14); // length of REGISTER_PATH
                info.watchedDirectories.push_back(responsePath);
            }
            else if (StringUtil::begins_with(response, wstring(L"STRING:"))) {
                logger << "received: STRING" << std::endl;
                wstring stringName, stringValue;
                if (!StringUtil::extractChunks(response, stringName, stringValue))
                    continue;
                if (stringName == L"CONTEXT_MENU_TITLE")
                    info.contextMenuTitle = move(stringValue);
            } else if (StringUtil::begins_with(response, wstring(L"MENU_ITEM:"))) {
                logger << "received: MENU_ITEM" << std::endl;
                wstring commandName, flags, title;
                if (!StringUtil::extractChunks(response, commandName, flags, title))
                    continue;
                info.menuItems.push_back({ commandName, flags, title });
            } else if (StringUtil::begins_with(response, wstring(L"GET_MENU_ITEMS:BEGIN"))) {
                receivedReplyFromDesktopClient = true;
                continue;
            } else if (StringUtil::begins_with(response, wstring(L"GET_MENU_ITEMS:END"))) {
                logger << "received: GET_MENU_ITEMS:END" << std::endl;
                break; // Stop once we completely received the last sent request
            } else {
                logger << "received: another reply" << std::endl;
            }
        } else {
            logger << "received nothing" << std::endl;
            Sleep(50);
            ++sleptCount;
        }
    }

    return info;
}

void NCClientInterface::SendRequest(const wchar_t *verb, const std::wstring &path, std::ofstream &logger)
{
    auto pipename = CommunicationSocket::DefaultPipePath();

    CommunicationSocket socket;
    if (!WaitNamedPipe(pipename.data(), PIPE_TIMEOUT)) {
        return;
    }
    if (!socket.Connect(pipename)) {
        return;
    }

    socket.SendMsg((verb + (L":" + path + L"\n")).data(), logger);
}
