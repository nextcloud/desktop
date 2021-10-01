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

#include "OCClientInterface.h"

#include "CommunicationSocket.h"
#include "StringUtil.h"

#include <shlobj.h>

#include <Strsafe.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>
#include <unordered_set>

// gdiplus min/max
// don't use std yet, as gdiplus will cause issues
using std::max;
using std::min;
#include <gdiplus.h>
using namespace std;

#include <comdef.h>
#include <wincrypt.h>
#include <shlwapi.h>
#include <wrl/client.h>

#include "../3rdparty/nlohmann-json/json.hpp"

using Microsoft::WRL::ComPtr;

#define PIPE_TIMEOUT  5*1000 //ms

namespace {

template <typename T = wstring>
void log(const wstring &msg, const T &error = {})
{
    wstringstream tmp;
    tmp << L"ownCloud: " << msg;
    if (!error.empty()) {
        tmp << L" " << error.data();
    }
    OutputDebugStringW(tmp.str().data());
}
void logWinError(const wstring &msg, const DWORD &error = GetLastError())
{
    log(msg, wstring(_com_error(error).ErrorMessage()));
}

void sendV2(const CommunicationSocket &socket, const wstring &command, const nlohmann::json &j)
{
    static int messageId = 0;
    const nlohmann::json json { { "id", to_string(messageId++) }, { "arguments", j } };
    const auto data = json.dump();
    wstringstream tmp;
    tmp << command << L":" << StringUtil::toUtf16(data.data(), data.size()) << L"\n";
    socket.SendMsg(tmp.str());
}

pair<wstring, nlohmann::json> parseV2(const wstring &data)
{
    const auto index = data.find(L":");
    const auto argStart = data.cbegin() + index + 1;
    const auto cData = StringUtil::toUtf8(&*argStart, distance(argStart, data.cend()));
    return { data.substr(0, index), nlohmann::json::parse(cData) };
}

std::shared_ptr<HBITMAP> saveImage(const string &data)
{
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    DWORD size = 2 * 1024;
    std::vector<BYTE> buf(size, 0);
    DWORD skipped;
    if (!CryptStringToBinaryA(data.data(), 0, CRYPT_STRING_BASE64, buf.data(), &size, &skipped, nullptr)) {
        logWinError(L"Failed to decode icon");
        return {};
    }
    ComPtr<IStream> stream = SHCreateMemStream(buf.data(), size);
    if (!stream) {
        log(L"Failed to create stream");
        return {};
    };
    HBITMAP result;
    Gdiplus::Bitmap bitmap(stream.Get(), true);
    const auto status = bitmap.GetHBITMAP(0, &result);
    if (status != Gdiplus::Ok) {
        log(L"Failed to get HBITMAP", to_wstring(status));
        return {};
    }
    return std::shared_ptr<HBITMAP> { new HBITMAP(result), [gdiplusToken](auto o) {
                                         DeleteObject(o);
                                         Gdiplus::GdiplusShutdown(gdiplusToken);
                                     } };
}
}

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
    sendV2(socket, L"V2/GET_CLIENT_ICON", { { "size", 16 } });
    socket.SendMsg(L"GET_STRINGS:CONTEXT_MENU_TITLE\n");
    socket.SendMsg(L"GET_MENU_ITEMS:" + files + L"\n");

    ContextMenuInfo info;
    std::wstring response;
    int sleptCount = 0;
    while (sleptCount < 5) {
        if (socket.ReadLine(&response)) {
            if (StringUtil::begins_with(response, wstring(L"V2/"))) {
                const auto msg = parseV2(response);
                const auto &arguments = msg.second["arguments"];
                if (msg.first == L"V2/GET_CLIENT_ICON_RESULT") {
                    if (arguments.contains("error")) {
                        log(L"V2/GET_CLIENT_ICON failed", arguments["error"].get<string>());
                    } else {
                        info.icon = saveImage(arguments["png"].get<string>());
                    }
                }

            } else if (StringUtil::begins_with(response, wstring(L"REGISTER_PATH:"))) {
                wstring responsePath = response.substr(14); // length of REGISTER_PATH
                info.watchedDirectories.push_back(responsePath);
            } else if (StringUtil::begins_with(response, wstring(L"STRING:"))) {
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
        }
        else {
            Sleep(50);
            ++sleptCount;
        }
    }
    return info;
}

void OCClientInterface::SendRequest(const wstring &verb, const std::wstring &path)
{
    auto pipename = CommunicationSocket::DefaultPipePath();

    CommunicationSocket socket;
    if (!WaitNamedPipe(pipename.data(), PIPE_TIMEOUT)) {
        return;
    }
    if (!socket.Connect(pipename)) {
        return;
    }

    socket.SendMsg(verb + L":" + path + L"\n");
}
