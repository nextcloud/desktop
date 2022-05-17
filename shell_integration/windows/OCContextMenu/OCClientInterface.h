/**
* Copyright (c) 2015 ownCloud GmbH. All rights reserved.
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

#include <windows.h>

class CommunicationSocket;

class OCClientInterface
{
public:
    struct ContextMenuInfo {
        std::vector<std::wstring> watchedDirectories;
        std::wstring contextMenuTitle;
        std::shared_ptr<HBITMAP> icon;
        struct MenuItem
        {
            std::wstring command, flags, title;
        };
        std::vector<MenuItem> menuItems;
    };
    static ContextMenuInfo FetchInfo(const std::wstring &files);
    [[nodiscard]] static bool SendRequest(const std::wstring &verb, const std::wstring &path);
};

#endif //ABSTRACTSOCKETHANDLER_H
