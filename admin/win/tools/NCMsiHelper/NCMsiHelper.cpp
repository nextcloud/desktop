/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "NCMsiHelper.h"
#include "utility.h"
#include "LogResult.h"

using namespace NCTools;

HRESULT NCMSIHELPER_API DoExecNsisUninstaller(int argc, LPWSTR *argv)
{
    if (argc != 2) {
        return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
    }

    const auto appShortName = std::wstring(argv[0]);
    const auto uninstallExePath = std::wstring(argv[1]);

    if (appShortName.empty()
         || uninstallExePath.empty()) {
        return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
    }

    auto appInstallDir = uninstallExePath;
    const auto posLastSlash = appInstallDir.find_last_of(PathSeparator);
    if (posLastSlash != std::wstring::npos) {
        appInstallDir.erase(posLastSlash);
    } else {
        return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
    }

    // Run uninstaller
    const std::wstring cmd = L'\"' + uninstallExePath + L"\" /S _?=" + appInstallDir;
    LogResult(S_OK, "Running '%ls'.", cmd.data());
    Utility::execCmd(cmd);

    LogResult(S_OK, "Waiting for NSIS uninstaller.");

    // Can't wait for the process because Uninstall.exe (opposed to Setup.exe) immediately returns, so we'll sleep a bit.
    Utility::waitForNsisUninstaller(appShortName);

    LogResult(S_OK, "Removing the NSIS uninstaller.");

    // Sleep a bit and clean up the NSIS mess
    Sleep(1500);
    DeleteFile(uninstallExePath.data());
    RemoveDirectory(appInstallDir.data());

    LogResult(S_OK, "Finished.");

    return S_OK;
}

HRESULT NCMSIHELPER_API DoRemoveNavigationPaneEntries(int argc, LPWSTR *argv)
{
    if (argc != 1) {
        return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
    }

    const auto appName = std::wstring(argv[0]);

    if (appName.empty()) {
        return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
    }

    LogResult(S_OK, "Removing '%ls' sync folders from Explorer's Navigation Pane for the current user.", appName.data());

    Utility::removeNavigationPaneEntries(appName);

    LogResult(S_OK, "Finished.");

    return S_OK;
}
