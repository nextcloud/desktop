/*
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

#include "utility.h"
#include <cassert>
#include <algorithm>
#include <Shlobj.h>
#include <psapi.h>

#define ASSERT assert
#define Q_ASSERT assert

namespace NCTools {

// Ported from libsync

registryVariant Utility::registryGetKeyValue(HKEY hRootKey, const std::wstring &subKey, const std::wstring &valueName)
{
    registryVariant value;

    HKEY hKey;

    REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.data()), 0, sam, &hKey);
    ASSERT(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    if (result != ERROR_SUCCESS)
        return value;

    DWORD type = 0, sizeInBytes = 0;
    result = RegQueryValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.data()), 0, &type, nullptr, &sizeInBytes);
    ASSERT(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    if (result == ERROR_SUCCESS) {
        switch (type) {
        case REG_DWORD:
            DWORD dword;
            Q_ASSERT(sizeInBytes == sizeof(dword));
            if (RegQueryValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.data()), 0, &type, reinterpret_cast<LPBYTE>(&dword), &sizeInBytes) == ERROR_SUCCESS) {
                value = int(dword);
            }
            break;
        case REG_EXPAND_SZ:
        case REG_SZ: {
            std::wstring string;
            string.resize(sizeInBytes / sizeof(wchar_t));
            result = RegQueryValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.data()), 0, &type, reinterpret_cast<LPBYTE>(string.data()), &sizeInBytes);

            if (result == ERROR_SUCCESS) {
                int newCharSize = sizeInBytes / sizeof(wchar_t);
                // From the doc:
                // If the data has the REG_SZ, REG_MULTI_SZ or REG_EXPAND_SZ type, the string may not have been stored with
                // the proper terminating null characters. Therefore, even if the function returns ERROR_SUCCESS,
                // the application should ensure that the string is properly terminated before using it; otherwise, it may overwrite a buffer.
                if (string.at(newCharSize - 1) == wchar_t('\0'))
                    string.resize(newCharSize - 1);
                value = string;
            }
            break;
        }
        case REG_BINARY: {
            std::vector<unsigned char> buffer;
            buffer.resize(sizeInBytes);
            result = RegQueryValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.data()), 0, &type, reinterpret_cast<LPBYTE>(buffer.data()), &sizeInBytes);
            if (result == ERROR_SUCCESS) {
                value = buffer.at(12);
            }
            break;
        }
        default:
            break;// Q_UNREACHABLE();
        }
    }
    ASSERT(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);

    RegCloseKey(hKey);
    return value;
}

bool Utility::registrySetKeyValue(HKEY hRootKey, const std::wstring &subKey, const std::wstring &valueName, DWORD type, const registryVariant &value)
{
    HKEY hKey;
    // KEY_WOW64_64KEY is necessary because CLSIDs are "Redirected and reflected only for CLSIDs that do not specify InprocServer32 or InprocHandler32."
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa384253%28v=vs.85%29.aspx#redirected__shared__and_reflected_keys_under_wow64
    // This shouldn't be an issue in our case since we use shell32.dll as InprocServer32, so we could write those registry keys for both 32 and 64bit.
    // FIXME: Not doing so at the moment means that explorer will show the cloud provider, but 32bit processes' open dialogs (like the ownCloud client itself) won't show it.
    REGSAM sam = KEY_WRITE | KEY_WOW64_64KEY;
    LONG result = RegCreateKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.data()), 0, nullptr, 0, sam, nullptr, &hKey, nullptr);
    ASSERT(result == ERROR_SUCCESS);
    if (result != ERROR_SUCCESS)
        return false;

    result = -1;
    switch (type) {
    case REG_DWORD: {
        try {
            DWORD dword = std::get<int>(value);
            result = RegSetValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.data()), 0, type, reinterpret_cast<const BYTE *>(&dword), sizeof(dword));
        }
        catch (const std::bad_variant_access&) {}
        break;
    }
    case REG_EXPAND_SZ:
    case REG_SZ: {
        try {
            std::wstring string = std::get<std::wstring>(value);
            result = RegSetValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.data()), 0, type, reinterpret_cast<const BYTE *>(string.data()), static_cast<DWORD>((string.size() + 1) * sizeof(wchar_t)));
        }
        catch (const std::bad_variant_access&) {}
        break;
    }
    default:
        break;// Q_UNREACHABLE();
    }
    ASSERT(result == ERROR_SUCCESS);

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool Utility::registryDeleteKeyTree(HKEY hRootKey, const std::wstring &subKey)
{
    HKEY hKey;
    REGSAM sam = DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.data()), 0, sam, &hKey);
    ASSERT(result == ERROR_SUCCESS);
    if (result != ERROR_SUCCESS)
        return false;

    result = RegDeleteTree(hKey, nullptr);
    RegCloseKey(hKey);
    ASSERT(result == ERROR_SUCCESS);

    result |= RegDeleteKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.data()), sam, 0);
    ASSERT(result == ERROR_SUCCESS);

    return result == ERROR_SUCCESS;
}

bool Utility::registryDeleteKeyValue(HKEY hRootKey, const std::wstring &subKey, const std::wstring &valueName)
{
    HKEY hKey;
    REGSAM sam = KEY_WRITE | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.data()), 0, sam, &hKey);
    ASSERT(result == ERROR_SUCCESS);
    if (result != ERROR_SUCCESS)
        return false;

    result = RegDeleteValue(hKey, reinterpret_cast<LPCWSTR>(valueName.data()));
    ASSERT(result == ERROR_SUCCESS);

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool Utility::registryWalkSubKeys(HKEY hRootKey, const std::wstring &subKey, const std::function<void(HKEY, const std::wstring&)> &callback)
{
    HKEY hKey;
    REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.data()), 0, sam, &hKey);
    ASSERT(result == ERROR_SUCCESS);
    if (result != ERROR_SUCCESS)
        return false;

    DWORD maxSubKeyNameSize;
    // Get the largest keyname size once instead of relying each call on ERROR_MORE_DATA.
    result = RegQueryInfoKey(hKey, nullptr, nullptr, nullptr, nullptr, &maxSubKeyNameSize, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    ASSERT(result == ERROR_SUCCESS);
    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false;
    }

    std::wstring subKeyName;
    subKeyName.reserve(maxSubKeyNameSize + 1);

    DWORD retCode = ERROR_SUCCESS;
    for (DWORD i = 0; retCode == ERROR_SUCCESS; ++i) {
        Q_ASSERT(unsigned(subKeyName.capacity()) > maxSubKeyNameSize);
        // Make the previously reserved capacity official again.
        subKeyName.resize(subKeyName.capacity());
        DWORD subKeyNameSize = static_cast<DWORD>(subKeyName.size());
        retCode = RegEnumKeyEx(hKey, i, reinterpret_cast<LPWSTR>(subKeyName.data()), &subKeyNameSize, nullptr, nullptr, nullptr, nullptr);

        ASSERT(result == ERROR_SUCCESS || retCode == ERROR_NO_MORE_ITEMS);
        if (retCode == ERROR_SUCCESS) {
            // subKeyNameSize excludes the trailing \0
            subKeyName.resize(subKeyNameSize);
            // Pass only the sub keyname, not the full path.
            callback(hKey, subKeyName);
        }
    }

    RegCloseKey(hKey);
    return retCode != ERROR_NO_MORE_ITEMS;
}

// Created for Win32

DWORD Utility::execCmd(std::wstring cmd, bool wait)
{
    // https://docs.microsoft.com/en-us/windows/win32/procthread/creating-processes
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Start the child process. 
    if (!CreateProcess(nullptr,  // No module name (use command line)
        cmd.data(),              // Command line
        nullptr,        // Process handle not inheritable
        nullptr,        // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        nullptr,        // Use parent's environment block
        nullptr,        // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi)            // Pointer to PROCESS_INFORMATION structure
        )
    {
        return ERROR_INVALID_FUNCTION;
    }

    DWORD exitCode = 0;

    if (wait) {
        // Wait until child process exits.
        WaitForSingleObject(pi.hProcess, INFINITE);

        GetExitCodeProcess(pi.hProcess, &exitCode);
    }

    // Close process and thread handles. 
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode;
}

bool Utility::killProcess(const std::wstring &exePath)
{
    // https://docs.microsoft.com/en-us/windows/win32/psapi/enumerating-all-processes
    // Get the list of process identifiers.
    DWORD aProcesses[1024], cbNeeded, cProcesses, i;

    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
        return false;
    }

    // Calculate how many process identifiers were returned.
    cProcesses = cbNeeded / sizeof(DWORD);

    std::wstring tmpMatch = exePath;
    std::transform(tmpMatch.begin(), tmpMatch.end(), tmpMatch.begin(), std::tolower);

    for (i = 0; i < cProcesses; i++) {
        if (aProcesses[i] != 0) {
            // Get a handle to the process.
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, FALSE, aProcesses[i]);

            // Get the process name.
            if (hProcess) {
                TCHAR szProcessName[MAX_PATH] = {0};
                DWORD cbSize = sizeof(szProcessName) / sizeof(TCHAR);

                if (QueryFullProcessImageName(hProcess, 0, szProcessName, &cbSize) == TRUE && cbSize > 0) {
                    std::wstring procName = szProcessName;
                    std::transform(procName.begin(), procName.end(), procName.begin(), std::tolower);

                    if (procName == tmpMatch) {
                        if (TerminateProcess(hProcess, 0) == TRUE) {
                            WaitForSingleObject(hProcess, INFINITE);
                            CloseHandle(hProcess);
                            return true;
                        }
                    }
                }

                CloseHandle(hProcess);
            }
        }
    }

    return false;
}

bool Utility::isValidDirectory(const std::wstring &path)
{
    auto attrib = GetFileAttributes(path.data());

    if (attrib == INVALID_FILE_ATTRIBUTES || GetLastError() == ERROR_FILE_NOT_FOUND) {
        return false;
    }

    return (attrib & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring Utility::getAppRegistryString(const std::wstring &appVendor, const std::wstring &appName, const std::wstring &valueName)
{
    std::wstring appKey = std::wstring(LR"(SOFTWARE\)") + appVendor + L'\\' + appName;
    std::wstring appKeyWow64 = std::wstring(LR"(SOFTWARE\WOW6432Node\)") + appVendor + L'\\' + appName;

    std::vector<std::wstring> appKeys = { appKey, appKeyWow64 };

    for (auto &key : appKeys) {
        try {
            return std::get<std::wstring>(Utility::registryGetKeyValue(HKEY_LOCAL_MACHINE,
                key,
                valueName));
        }
        catch (const std::bad_variant_access&) {}
    }

    return {};
}

std::wstring Utility::getAppPath(const std::wstring &appVendor, const std::wstring &appName)
{
    return getAppRegistryString(appVendor, appName, L""); // intentionally left empty to get the key's "(default)" value
}

std::wstring Utility::getConfigPath(const std::wstring &appName)
{
    // On Windows, use AppDataLocation, that's where the roaming data is and where we should store the config file
    PWSTR pszPath = nullptr;
    if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &pszPath)) || !pszPath) {
        return {};
    }
    std::wstring path = pszPath + PathSeparator + appName + PathSeparator;
    CoTaskMemFree(pszPath);
             
    auto newLocation = path;

    return newLocation;
}

void Utility::waitForNsisUninstaller(const std::wstring &appShortName)
{
    // Can't WaitForSingleObject because NSIS Uninstall.exe copies itself to a TEMP directory and creates a new process,
    // so we do sort of a hack and wait for its mutex (see nextcloud.nsi).
    HANDLE hMutex;
    DWORD lastError = ERROR_SUCCESS;
    std::wstring name = appShortName + std::wstring(L"Uninstaller");

    // Give the process enough time to start, to wait for the NSIS mutex.
    Sleep(1500);

    do {
        hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, name.data());
        lastError = GetLastError();
        if (hMutex) {
            CloseHandle(hMutex);
        }

        // This is sort of a hack because WaitForSingleObject immediately returns for the NSIS mutex.
        Sleep(500);
    } while (lastError != ERROR_FILE_NOT_FOUND);
}

void Utility::removeNavigationPaneEntries(const std::wstring &appName)
{
    if (appName.empty()) {
        return;
    }

    // Start by looking at every registered namespace extension for the sidebar, and look for an "ApplicationName" value
    // that matches ours when we saved.
    std::vector<std::wstring> entriesToRemove;
    Utility::registryWalkSubKeys(
        HKEY_CURRENT_USER,
        LR"(Software\Microsoft\Windows\CurrentVersion\Explorer\Desktop\NameSpace)",
        [&entriesToRemove, &appName](HKEY key, const std::wstring &subKey) {
            try {
                auto curAppName = std::get<std::wstring>(Utility::registryGetKeyValue(key, subKey, L"ApplicationName"));

                if (curAppName == appName) {
                    entriesToRemove.push_back(subKey);
                }
            }
            catch (const std::bad_variant_access&) {}
        });

    for (auto &clsid : entriesToRemove) {
        std::wstring clsidStr = clsid;
        std::wstring clsidPath = std::wstring(LR"(Software\Classes\CLSID\)") + clsidStr;
        std::wstring clsidPathWow64 = std::wstring(LR"(Software\Classes\Wow6432Node\CLSID\)") + clsidStr;
        std::wstring namespacePath = std::wstring(LR"(Software\Microsoft\Windows\CurrentVersion\Explorer\Desktop\NameSpace\)") + clsidStr;

        Utility::registryDeleteKeyTree(HKEY_CURRENT_USER, clsidPath);
        Utility::registryDeleteKeyTree(HKEY_CURRENT_USER, clsidPathWow64);
        Utility::registryDeleteKeyTree(HKEY_CURRENT_USER, namespacePath);
        Utility::registryDeleteKeyValue(HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\CurrentVersion\Explorer\HideDesktopIcons\NewStartPanel)", clsidStr);
    }
}

// Ported from gui, modified to optionally rename matching files
bool Utility::copy_dir_recursive(std::wstring from_dir, std::wstring to_dir, copy_dir_recursive_callback *callbackFileNameMatchReplace)
{
    WIN32_FIND_DATA fileData;

    if (from_dir.empty() || to_dir.empty()) {
        return false;
    }

    if (from_dir.back() != PathSeparator.front())
        from_dir.append(PathSeparator);
    if (to_dir.back() != PathSeparator.front())
        to_dir.append(PathSeparator);

    std::wstring startDir = from_dir;
    startDir.append(L"*.*");

    auto hFind = FindFirstFile(startDir.data(), &fileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool success = true;

    do {
        if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (std::wstring(fileData.cFileName) == L"." || std::wstring(fileData.cFileName) == L"..") {
                continue;
            }

            std::wstring from = from_dir + fileData.cFileName;
            std::wstring to = to_dir + fileData.cFileName;

            if (CreateDirectoryEx(from.data(), to.data(), nullptr) == FALSE) {
                success = false;
                break;
            }

            if (copy_dir_recursive(from, to, callbackFileNameMatchReplace) == false) {
                success = false;
                break;
            }
        } else {
            std::wstring newFilename = fileData.cFileName;

            if (callbackFileNameMatchReplace) {
                (*callbackFileNameMatchReplace)(std::wstring(fileData.cFileName), newFilename);
            }

            std::wstring from = from_dir + fileData.cFileName;
            std::wstring to = to_dir + newFilename;

            if (CopyFile(from.data(), to.data(), TRUE) == FALSE) {
                success = false;
                break;
            }
        }
    } while (FindNextFile(hFind, &fileData));

    FindClose(hFind);

    return success;
}

} // namespace NCTools
