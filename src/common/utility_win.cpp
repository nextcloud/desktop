/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "asserts.h"
#include "utility.h"
#include "gui/configgui.h"
#include "config.h"

#include <comdef.h>
#include <Lmcons.h>
#include <psapi.h>
#include <RestartManager.h>
#include <shlguid.h>
#include <shlobj.h>
#include <string>
#include <winbase.h>
#include <windows.h>
#include <winerror.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLibrary>
#include <QSettings>
#include <QTemporaryFile>
#include <QFileInfo>

extern Q_CORE_EXPORT int qt_ntfs_permission_lookup;

static const char systemRunPathC[] = R"(HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Run)";
static const char runPathC[] = R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)";

namespace OCC {

QVector<Utility::ProcessInfosForOpenFile> Utility::queryProcessInfosKeepingFileOpen(const QString &filePath)
{
    QVector<ProcessInfosForOpenFile> results;

    DWORD restartManagerSession = 0;
    WCHAR restartManagerSessionKey[CCH_RM_SESSION_KEY + 1] = {0};
    auto errorStatus = RmStartSession(&restartManagerSession, 0, restartManagerSessionKey);
    if (errorStatus != ERROR_SUCCESS) {
        return results;
    }

    LPCWSTR files[] = {reinterpret_cast<LPCWSTR>(filePath.utf16())};
    errorStatus = RmRegisterResources(restartManagerSession, 1, files, 0, NULL, 0, NULL);
    if (errorStatus != ERROR_SUCCESS) {
        RmEndSession(restartManagerSession);
        return results;
    }

    DWORD rebootReasons = 0;
    UINT rmProcessInfosNeededCount = 0;
    std::vector<RM_PROCESS_INFO> rmProcessInfos;
    auto rmProcessInfosRequestedCount = static_cast<UINT>(rmProcessInfos.size());
    errorStatus = RmGetList(restartManagerSession, &rmProcessInfosNeededCount, &rmProcessInfosRequestedCount, rmProcessInfos.data(), &rebootReasons);
    
    if (errorStatus == ERROR_MORE_DATA) {
        rmProcessInfos.resize(rmProcessInfosNeededCount, {});
        rmProcessInfosRequestedCount = static_cast<UINT>(rmProcessInfos.size());
        errorStatus = RmGetList(restartManagerSession, &rmProcessInfosNeededCount, &rmProcessInfosRequestedCount, rmProcessInfos.data(), &rebootReasons);
    }
    
    if (errorStatus != ERROR_SUCCESS || rmProcessInfos.empty()) {
        RmEndSession(restartManagerSession);
        return results;
    }

    for (size_t i = 0; i < rmProcessInfos.size(); ++i) {
        const auto processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, rmProcessInfos[i].Process.dwProcessId);
        if (!processHandle) {
            continue;
        }

        FILETIME ftCreate, ftExit, ftKernel, ftUser;

        if (!GetProcessTimes(processHandle, &ftCreate, &ftExit, &ftKernel, &ftUser)
            || CompareFileTime(&rmProcessInfos[i].Process.ProcessStartTime, &ftCreate) != 0) {
            CloseHandle(processHandle);
            continue;
        }

        WCHAR processFullPath[MAX_PATH];
        DWORD processFullPathLength = MAX_PATH;
        if (QueryFullProcessImageNameW(processHandle, 0, processFullPath, &processFullPathLength) && processFullPathLength <= MAX_PATH) {
            const auto processFullPathString = QDir::fromNativeSeparators(QString::fromWCharArray(processFullPath));
            const QFileInfo fileInfoForProcess(processFullPathString);
            const auto processName = fileInfoForProcess.fileName();
            if (!processName.isEmpty()) {
                results.push_back(Utility::ProcessInfosForOpenFile{rmProcessInfos[i].Process.dwProcessId, processName});
            }
        }
        CloseHandle(processHandle);
    }
    RmEndSession(restartManagerSession);

    return results;
}

QString Utility::systemPathToLinks()
{
    static const QString pathToLinks = [] {
        PWSTR path = nullptr;
        if (SHGetKnownFolderPath(FOLDERID_Links, 0, nullptr, &path) != S_OK) {
            qCWarning(lcUtility) << "SHGetKnownFolderPath failed.";
            CoTaskMemFree(path);
            return QString();
        }

        const auto links = QDir::fromNativeSeparators(QString::fromWCharArray(path));
        CoTaskMemFree(path);
        return links;
    }();

    return pathToLinks;
}

void Utility::setupDesktopIni(const QString &folder, const QString localizedResourceName)
{
    // First create a Desktop.ini so that the folder and favorite link show our application's icon.
    QFile desktopIni(folder + QLatin1String("/Desktop.ini"));
    const auto migration = !localizedResourceName.isEmpty();
    if (!migration && desktopIni.exists()) {
        qCWarning(lcUtility) << desktopIni.fileName() << "already exists, not overwriting it to set the folder icon.";
        return;
    }

    qCDebug(lcUtility) << "Creating" << desktopIni.fileName() << "to set a folder icon in Explorer.";
    desktopIni.open(QFile::WriteOnly);
    desktopIni.write("[.ShellClassInfo]\r\nIconResource=");
    desktopIni.write(QDir::toNativeSeparators(qApp->applicationFilePath()).toUtf8());
#ifdef APPLICATION_FOLDER_ICON_INDEX
    const auto iconIndex = APPLICATION_FOLDER_ICON_INDEX;
#else
    const auto iconIndex = "0";
#endif
    desktopIni.write(",");
    desktopIni.write(iconIndex);
    if (migration) {
        desktopIni.write("\r\nLocalizedResourceName=");
        desktopIni.write(localizedResourceName.toUtf8());
    }
    desktopIni.write("\r\n");
    desktopIni.close();

    // Set the folder as system and Desktop.ini as hidden+system for explorer to pick it.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/cc144102
    DWORD folderAttrs = GetFileAttributesW((wchar_t *)folder.utf16());
    SetFileAttributesW((wchar_t *)folder.utf16(), folderAttrs | FILE_ATTRIBUTE_SYSTEM);
    SetFileAttributesW((wchar_t *)desktopIni.fileName().utf16(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
}

void Utility::setupFavLink(const QString &folder)
{
    // create Desktop.ini
    setupDesktopIni(folder);

    const auto pathToLinks = systemPathToLinks();
    if (pathToLinks.isEmpty()) {
        qCWarning(lcUtility) << "SHGetKnownFolderPath for " << folder << "has failed.";
        return;
    }

    const QDir folderDir(QDir::fromNativeSeparators(folder));
    const QString filePath = folderDir.dirName() + QLatin1String(".lnk");
    const auto linkName = QDir().filePath(filePath);

    qCDebug(lcUtility) << "Creating favorite link from" << folder << "to" << linkName;
    if (!QFile::link(folder, linkName)) {
        qCWarning(lcUtility) << "linking" << folder << "to" << linkName << "failed!";
    }
}

QString Utility::syncFolderDisplayName(const QString &currentDisplayName, const QString &newName)
{
    const auto nextcloud = QStringLiteral("Nextcloud");
    if (!currentDisplayName.startsWith(nextcloud)) {
        qCWarning(lcUtility) << "Nothings needs to be rename for" << currentDisplayName;
        return currentDisplayName;
    }

    QString digits;
    for (auto letter = std::crbegin(currentDisplayName); letter != std::crend(currentDisplayName); ++letter) {
        if (!letter->isDigit()) {
            break;
        }
        digits.prepend(*letter);
    }

    return newName + digits;
}

void Utility::migrateFavLink(const QString &folder)
{
    const QDir folderDir(QDir::fromNativeSeparators(folder));
    const auto oldDirName = folderDir.dirName();
    const auto folderDisplayName = syncFolderDisplayName(oldDirName, QStringLiteral(APPLICATION_NAME));
    // overwrite Desktop.ini, update icon, update folder display name
    setupDesktopIni(folder, folderDisplayName);

    const auto pathToLinks = systemPathToLinks();
    if (pathToLinks.isEmpty()) {
        qCWarning(lcUtility) << "SHGetKnownFolderPath for links has failed.";
        return;
    }

    const QDir dirPathToLinks(pathToLinks);
    const auto oldLnkFilename = dirPathToLinks.filePath(oldDirName + QLatin1String(".lnk"));
    const auto newLnkFilename = dirPathToLinks.filePath(folderDisplayName + QLatin1String(".lnk"));

    if (QFile::exists(newLnkFilename)) {
        qCWarning(lcUtility) << "New lnk file already exists" << newLnkFilename;
        return;
    }

    qCDebug(lcUtility) << "Renaming favorite link from" << oldLnkFilename << "to" << newLnkFilename;
    if (QFile::rename(oldLnkFilename, newLnkFilename)  && !QFile::rename(oldLnkFilename, newLnkFilename)) {
        qCWarning(lcUtility) << "renaming" << oldLnkFilename << "to" << newLnkFilename << "failed!";
    }
}

void Utility::removeFavLink(const QString &folder)
{
    const QDir folderDir(folder);

    // #1 Remove the Desktop.ini to reset the folder icon
    if (!QFile::remove(folderDir.absoluteFilePath(QLatin1String("Desktop.ini")))) {
        qCWarning(lcUtility) << "Remove Desktop.ini from" << folder
                             << " has failed. Make sure it exists and is not locked by another process.";
    }

    // #2 Remove the system file attribute
    const auto folderAttrs = GetFileAttributesW(folder.toStdWString().c_str());
    if (!SetFileAttributesW(folder.toStdWString().c_str(), folderAttrs & ~FILE_ATTRIBUTE_SYSTEM)) {
        qCWarning(lcUtility) << "Remove system file attribute failed for:" << folder;
    }

    // #3 Remove the link to this folder
    PWSTR path;
    if (!SHGetKnownFolderPath(FOLDERID_Links, 0, nullptr, &path) == S_OK) {
        qCWarning(lcUtility) << "SHGetKnownFolderPath for " << folder << "has failed.";
        return;
    }

    const QDir links(QString::fromWCharArray(path));
    CoTaskMemFree(path);

    const auto folderDisplayName = syncFolderDisplayName(folderDir.dirName(), QStringLiteral(APPLICATION_NAME));
    const auto linkName = QDir(links).absoluteFilePath(folderDisplayName + QLatin1String(".lnk"));

    qCDebug(lcUtility) << "Removing favorite link from" << folder << "to" << linkName;
    if (!QFile::remove(linkName)) {
        qCWarning(lcUtility) << "Removing a favorite link from" << folder << "to" << linkName << "failed.";
    }
}

bool Utility::hasSystemLaunchOnStartup(const QString &appName)
{
    QString runPath = QLatin1String(systemRunPathC);
    QSettings settings(runPath, QSettings::NativeFormat);
    return settings.contains(appName);
}

bool Utility::hasLaunchOnStartup(const QString &appName)
{
    QString runPath = QLatin1String(runPathC);
    QSettings settings(runPath, QSettings::NativeFormat);
    return settings.contains(appName);
}

void Utility::setLaunchOnStartup(const QString &appName, const QString &guiName, bool enable)
{
    Q_UNUSED(guiName);
    QString runPath = QLatin1String(runPathC);
    QSettings settings(runPath, QSettings::NativeFormat);
    if (enable) {
        settings.setValue(appName, QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    } else {
        settings.remove(appName);
    }
}

bool Utility::hasDarkSystray()
{
    if(Utility::registryGetKeyValue(    HKEY_CURRENT_USER,
                                        QStringLiteral(R"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)"),
                                        QStringLiteral("SystemUsesLightTheme") ) == 1) {
        return false;
    }
    else {
        return true;
    }
}

QRect Utility::getTaskbarDimensions()
{
    APPBARDATA barData;
    barData.cbSize = sizeof(APPBARDATA);

    BOOL fResult = (BOOL)SHAppBarMessage(ABM_GETTASKBARPOS, &barData);
    if (!fResult) {
        return QRect();
    }

    RECT barRect = barData.rc;
    return QRect(barRect.left, barRect.top, (barRect.right - barRect.left), (barRect.bottom - barRect.top));
}

bool Utility::registryKeyExists(HKEY hRootKey, const QString &subKey)
{
    HKEY hKey;

    REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, sam, &hKey);

    RegCloseKey(hKey);
    return result != ERROR_FILE_NOT_FOUND;
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
                if (string.at(newCharSize - 1) == QLatin1Char('\0'))
                    string.resize(newCharSize - 1);
                value = string;
            }
            break;
        }
        case REG_BINARY: {
            QByteArray buffer;
            buffer.resize(sizeInBytes);
            result = RegQueryValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()), 0, &type, reinterpret_cast<LPBYTE>(buffer.data()), &sizeInBytes);
            if (result == ERROR_SUCCESS) {
                value = buffer.at(12);
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

bool Utility::registryWalkValues(HKEY hRootKey, const QString &subKey, const std::function<void(const QString &, bool *)> &callback)
{
    HKEY hKey;
    REGSAM sam = KEY_QUERY_VALUE;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, sam, &hKey);
    ASSERT(result == ERROR_SUCCESS);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    DWORD maxValueNameSize = 0;
    result = RegQueryInfoKey(hKey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &maxValueNameSize, nullptr, nullptr, nullptr);
    ASSERT(result == ERROR_SUCCESS);
    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false;
    }

    QString valueName;
    valueName.reserve(maxValueNameSize + 1);

    DWORD retCode = ERROR_SUCCESS;
    bool done = false;
    for (DWORD i = 0; retCode == ERROR_SUCCESS; ++i) {
        Q_ASSERT(unsigned(valueName.capacity()) > maxValueNameSize);
        valueName.resize(valueName.capacity());
        DWORD valueNameSize = valueName.size();
        retCode = RegEnumValue(hKey, i, reinterpret_cast<LPWSTR>(valueName.data()), &valueNameSize, nullptr, nullptr, nullptr, nullptr);

        ASSERT(result == ERROR_SUCCESS || retCode == ERROR_NO_MORE_ITEMS);
        if (retCode == ERROR_SUCCESS) {
            valueName.resize(valueNameSize);
            callback(valueName, &done);

            if (done) {
                break;
            }
        }
    }

    RegCloseKey(hKey);
    return retCode != ERROR_NO_MORE_ITEMS;
}

DWORD Utility::convertSizeToDWORD(size_t &convertVar)
{
    if( convertVar > UINT_MAX ) {
        //throw std::bad_cast();
        convertVar = UINT_MAX; // intentionally default to wrong value here to not crash: exception handling TBD
    }
    return static_cast<DWORD>(convertVar);
}

void Utility::UnixTimeToLargeIntegerFiletime(time_t t, LARGE_INTEGER *hundredNSecs)
{
    hundredNSecs->QuadPart = (t * 10000000LL) + 116444736000000000LL;
}

bool Utility::canCreateFileInPath(const QString &path)
{
    Q_ASSERT(!path.isEmpty());
    const auto pathWithSlash = !path.endsWith(QLatin1Char('/'))
        ? path + QLatin1Char('/')
        : path;
    QTemporaryFile testFile(pathWithSlash + QStringLiteral("~$write-test-file-XXXXXX"));
    return testFile.open();
}

QString Utility::formatWinError(long errorCode)
{
    return QStringLiteral("WindowsError: %1: %2").arg(QString::number(errorCode, 16), QString::fromWCharArray(_com_error(errorCode).ErrorMessage()));
}

QString Utility::getCurrentUserName()
{
    TCHAR username[UNLEN + 1] = {0};
    DWORD len = sizeof(username) / sizeof(TCHAR);
    
    if (!GetUserName(username, &len)) {
        const auto lastError = GetLastError();
        qCWarning(lcUtility).nospace() << "Could not retrieve Windows user name. errorMessage=" << formatWinError(lastError);
    }

    return QString::fromWCharArray(username);
}

void Utility::registerUriHandlerForLocalEditing() { /* URI handler is registered via Nextcloud.wxs */ }

Utility::NtfsPermissionLookupRAII::NtfsPermissionLookupRAII()
{
    qt_ntfs_permission_lookup++;
}

Utility::NtfsPermissionLookupRAII::~NtfsPermissionLookupRAII()
{
    qt_ntfs_permission_lookup--;
}

void Utility::HandleDeleter::operator()(HANDLE handle) const
{
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }

    ::CloseHandle(handle);
}

void Utility::LocalFreeDeleter::operator()(void *p) const
{
    if (!p) {
        return;
    }

    ::LocalFree(reinterpret_cast<HLOCAL>(p));
}

} // namespace OCC
