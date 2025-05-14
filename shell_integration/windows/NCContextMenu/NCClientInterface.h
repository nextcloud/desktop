/*
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef AbstractSocketHandler_H
#define AbstractSocketHandler_H

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <fstream>

class CommunicationSocket;

class NCClientInterface
{
public:
    struct ContextMenuInfo {
        std::vector<std::wstring> watchedDirectories;
        std::wstring contextMenuTitle;
        struct MenuItem
        {
            std::wstring command, flags, title;
        };
        std::vector<MenuItem> menuItems;
    };
    static ContextMenuInfo FetchInfo(const std::wstring &files, std::ofstream &logger);
    static void SendRequest(const wchar_t *verb, const std::wstring &path, std::ofstream &logger);
};

#endif //ABSTRACTSOCKETHANDLER_H
