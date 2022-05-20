/**
 * Copyright (c) 2022 Hannah von Reth <hannah.vonreth@owncloud.com>. All rights reserved.
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

#pragma once
#include <comdef.h>
#include <sstream>

namespace OCShell {

template <typename T = std::wstring>
void log(const std::wstring &msg, const T &error = {})
{
    std::wstringstream tmp;
    tmp << L"ownCloud ShellExtension: " << msg;
    if (!error.empty()) {
        tmp << L" " << error.data();
    }
    OutputDebugStringW(tmp.str().data());
}
inline void logWinError(const std::wstring &msg, const DWORD error = GetLastError())
{
    log(msg, std::wstring(_com_error(error).ErrorMessage()));
}

}