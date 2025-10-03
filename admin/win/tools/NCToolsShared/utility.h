/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 * Copyright (C) by Michael Schuster <michael@schuster.ms>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <variant>
#include <functional>

namespace NCTools {

typedef std::variant<int, std::wstring, std::vector<unsigned char>> registryVariant;

static const std::wstring PathSeparator = L"\\";

namespace Utility {
    // Ported from libsync
    registryVariant registryGetKeyValue(HKEY hRootKey, const std::wstring& subKey, const std::wstring& valueName);
    bool registrySetKeyValue(HKEY hRootKey, const std::wstring &subKey, const std::wstring &valueName, DWORD type, const registryVariant &value);
    bool registryDeleteKeyTree(HKEY hRootKey, const std::wstring &subKey);
    bool registryDeleteKeyValue(HKEY hRootKey, const std::wstring &subKey, const std::wstring &valueName);
    bool registryWalkSubKeys(HKEY hRootKey, const std::wstring &subKey, const std::function<void(HKEY, const std::wstring&)> &callback);

    // Ported from gui, modified to optionally rename matching files
    typedef std::function<void(const std::wstring&, std::wstring&)> copy_dir_recursive_callback;
    bool copy_dir_recursive(std::wstring from_dir, std::wstring to_dir, copy_dir_recursive_callback* callbackFileNameMatchReplace = nullptr);

    // Created for native Win32
    DWORD execCmd(std::wstring cmd, bool wait = true);
    bool killProcess(const std::wstring &exePath);
    bool isValidDirectory(const std::wstring &path);
    std::wstring getAppRegistryString(const std::wstring &appVendor, const std::wstring &appName, const std::wstring &valueName);
    std::wstring getAppPath(const std::wstring &appVendor, const std::wstring &appName);
    std::wstring getConfigPath(const std::wstring &appName);
    void waitForNsisUninstaller(const std::wstring& appShortName);
    void removeNavigationPaneEntries(const std::wstring &appName);
}

} // namespace NCTools
