/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
