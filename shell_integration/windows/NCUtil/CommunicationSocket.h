/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-FileCopyrightText: 2000-2013 Liferay, Inc. All rights reserved
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef COMMUNICATIONSOCKET_H
#define COMMUNICATIONSOCKET_H

#pragma once

#pragma warning (disable : 4251)

#include <string>
#include <vector>
#include <WinSock2.h>

class __declspec(dllexport) CommunicationSocket
{
public:
    static std::wstring DefaultPipePath();

    CommunicationSocket();
    ~CommunicationSocket();

    bool Connect(const std::wstring& pipename);
    bool Close();

    bool SendMsg(const wchar_t*) const;
    bool ReadLine(std::wstring*);

    HANDLE Event() { return _pipe; }

private:    
    HANDLE _pipe;
    std::vector<char> _buffer;
    bool _connected = false;
};

#endif
