/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include "asserts.h"
#include <shlobj.h>
#include <winbase.h>
#include <windows.h>
#include <winerror.h>
#include <shlguid.h>
#include <string>
#include <QLibrary>

static const char systemRunPathC[] = "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char runPathC[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";

namespace OCC {

static void setupFavLink_private(const QString &folder)
{
    // First create a Desktop.ini so that the folder and favorite link show our application's icon.
    QFile desktopIni(folder + QLatin1String("/Desktop.ini"));
    if (desktopIni.exists()) {
        qCWarning(lcUtility) << desktopIni.fileName() << "already exists, not overwriting it to set the folder icon.";
    } else {
        qCInfo(lcUtility) << "Creating" << desktopIni.fileName() << "to set a folder icon in Explorer.";
        desktopIni.open(QFile::WriteOnly);
        desktopIni.write("[.ShellClassInfo]\r\nIconResource=");
        desktopIni.write(QDir::toNativeSeparators(qApp->applicationFilePath()).toUtf8());
        desktopIni.write(",0\r\n");
        desktopIni.close();

        // Set the folder as system and Desktop.ini as hidden+system for explorer to pick it.
        // https://msdn.microsoft.com/en-us/library/windows/desktop/cc144102
        DWORD folderAttrs = GetFileAttributesW((wchar_t *)folder.utf16());
        SetFileAttributesW((wchar_t *)folder.utf16(), folderAttrs | FILE_ATTRIBUTE_SYSTEM);
        SetFileAttributesW((wchar_t *)desktopIni.fileName().utf16(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    }

    // Windows Explorer: Place under "Favorites" (Links)
    QString linkName;
    QDir folderDir(QDir::fromNativeSeparators(folder));

    /* Use new WINAPI functions */
    PWSTR path;

    if (SHGetKnownFolderPath(FOLDERID_Links, 0, NULL, &path) == S_OK) {
        QString links = QDir::fromNativeSeparators(QString::fromWCharArray(path));
        linkName = QDir(links).filePath(folderDir.dirName() + QLatin1String(".lnk"));
        CoTaskMemFree(path);
    }
    qCInfo(lcUtility) << "Creating favorite link from" << folder << "to" << linkName;
    if (!QFile::link(folder, linkName))
        qCWarning(lcUtility) << "linking" << folder << "to" << linkName << "failed!";
}

bool hasSystemLaunchOnStartup_private(const QString &appName)
{
    QString runPath = QLatin1String(systemRunPathC);
    QSettings settings(runPath, QSettings::NativeFormat);
    return settings.contains(appName);
}

bool hasLaunchOnStartup_private(const QString &appName)
{
    QString runPath = QLatin1String(runPathC);
    QSettings settings(runPath, QSettings::NativeFormat);
    return settings.contains(appName);
}

void setLaunchOnStartup_private(const QString &appName, const QString &guiName, bool enable)
{
    Q_UNUSED(guiName);
    QString runPath = QLatin1String(runPathC);
    QSettings settings(runPath, QSettings::NativeFormat);
    if (enable) {
        settings.setValue(appName, QCoreApplication::applicationFilePath().replace('/', '\\'));
    } else {
        settings.remove(appName);
    }
}

static inline bool hasDarkSystray_private()
{
    return true;
}

QVariant Utility::registryGetKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName)
{
    QVariant value;

    HKEY hKey;

    REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, sam, &hKey);
    ASSERT(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    if (result != ERROR_SUCCESS)
        return value;

    DWORD type = 0, sizeInBytes = 0;
    result = RegQueryValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()), 0, &type, nullptr, &sizeInBytes);
    ASSERT(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    if (result == ERROR_SUCCESS) {
        switch (type) {
        case REG_DWORD:
            DWORD dword;
            Q_ASSERT(sizeInBytes == sizeof(dword));
            if (RegQueryValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()), 0, &type, reinterpret_cast<LPBYTE>(&dword), &sizeInBytes) == ERROR_SUCCESS) {
                value = int(dword);
            }
            break;
        case REG_EXPAND_SZ:
        case REG_SZ: {
            QString string;
            string.resize(sizeInBytes / sizeof(QChar));
            result = RegQueryValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()), 0, &type, reinterpret_cast<LPBYTE>(string.data()), &sizeInBytes);

            if (result == ERROR_SUCCESS) {
                int newCharSize = sizeInBytes / sizeof(QChar);
                // From the doc:
                // If the data has the REG_SZ, REG_MULTI_SZ or REG_EXPAND_SZ type, the string may not have been stored with
                // the proper terminating null characters. Therefore, even if the function returns ERROR_SUCCESS,
                // the application should ensure that the string is properly terminated before using it; otherwise, it may overwrite a buffer.
                if (string.at(newCharSize - 1) == QChar('\0'))
                    string.resize(newCharSize - 1);
                value = string;
            }
            break;
        }
        default:
            Q_UNREACHABLE();
        }
    }
    ASSERT(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);

    RegCloseKey(hKey);
    return value;
}

bool Utility::registrySetKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName, DWORD type, const QVariant &value)
{
    HKEY hKey;
    // KEY_WOW64_64KEY is necessary because CLSIDs are "Redirected and reflected only for CLSIDs that do not specify InprocServer32 or InprocHandler32."
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa384253%28v=vs.85%29.aspx#redirected__shared__and_reflected_keys_under_wow64
    // This shouldn't be an issue in our case since we use shell32.dll as InprocServer32, so we could write those registry keys for both 32 and 64bit.
    // FIXME: Not doing so at the moment means that explorer will show the cloud provider, but 32bit processes' open dialogs (like the ownCloud client itself) won't show it.
    REGSAM sam = KEY_WRITE | KEY_WOW64_64KEY;
    LONG result = RegCreateKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, nullptr, 0, sam, nullptr, &hKey, nullptr);
    ASSERT(result == ERROR_SUCCESS);
    if (result != ERROR_SUCCESS)
        return false;

    result = -1;
    switch (type) {
    case REG_DWORD: {
        DWORD dword = value.toInt();
        result = RegSetValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()), 0, type, reinterpret_cast<const BYTE *>(&dword), sizeof(dword));
        break;
    }
    case REG_EXPAND_SZ:
    case REG_SZ: {
        QString string = value.toString();
        result = RegSetValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()), 0, type, reinterpret_cast<const BYTE *>(string.constData()), (string.size() + 1) * sizeof(QChar));
        break;
    }
    default:
        Q_UNREACHABLE();
    }
    ASSERT(result == ERROR_SUCCESS);

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool Utility::registryDeleteKeyTree(HKEY hRootKey, const QString &subKey)
{
    HKEY hKey;
    REGSAM sam = DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, sam, &hKey);
    ASSERT(result == ERROR_SUCCESS);
    if (result != ERROR_SUCCESS)
        return false;

    result = RegDeleteTree(hKey, nullptr);
    RegCloseKey(hKey);
    ASSERT(result == ERROR_SUCCESS);

    result |= RegDeleteKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), sam, 0);
    ASSERT(result == ERROR_SUCCESS);

    return result == ERROR_SUCCESS;
}

bool Utility::registryDeleteKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName)
{
    HKEY hKey;
    REGSAM sam = KEY_WRITE | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, sam, &hKey);
    ASSERT(result == ERROR_SUCCESS);
    if (result != ERROR_SUCCESS)
        return false;

    result = RegDeleteValue(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()));
    ASSERT(result == ERROR_SUCCESS);

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool Utility::registryWalkSubKeys(HKEY hRootKey, const QString &subKey, const std::function<void(HKEY, const QString &)> &callback)
{
    HKEY hKey;
    REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, sam, &hKey);
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

    QString subKeyName;
    subKeyName.reserve(maxSubKeyNameSize + 1);

    DWORD retCode = ERROR_SUCCESS;
    for (DWORD i = 0; retCode == ERROR_SUCCESS; ++i) {
        Q_ASSERT(unsigned(subKeyName.capacity()) > maxSubKeyNameSize);
        // Make the previously reserved capacity official again.
        subKeyName.resize(subKeyName.capacity());
        DWORD subKeyNameSize = subKeyName.size();
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

void Utility::UnixTimeToFiletime(time_t t, FILETIME *filetime)
{
    LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000;
    filetime->dwLowDateTime = (DWORD) ll;
    filetime->dwHighDateTime = ll >>32;
}

void Utility::FiletimeToLargeIntegerFiletime(FILETIME *filetime, LARGE_INTEGER *hundredNSecs)
{
    hundredNSecs->LowPart = filetime->dwLowDateTime;
    hundredNSecs->HighPart = filetime->dwHighDateTime;
}

void Utility::UnixTimeToLargeIntegerFiletime(time_t t, LARGE_INTEGER *hundredNSecs)
{
    LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000;
    hundredNSecs->LowPart = (DWORD) ll;
    hundredNSecs->HighPart = ll >>32;
}

} // namespace OCC
