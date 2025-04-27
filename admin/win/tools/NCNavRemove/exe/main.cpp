/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <windows.h>
#include "utility.h"
#include "SimpleNamedMutex.h"
#include "NavRemoveConstants.h"
#include "../ConfigIni.h"

using namespace NCTools;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // Mutex
    SimpleNamedMutex mutex(std::wstring(MUTEX_NAME));

    if (!mutex.lock()) {
        return 0;
    }

    // Config
    ConfigIni ini;

    if (!ini.load()) {
        return 1;
    }

    Utility::removeNavigationPaneEntries(ini.getAppName());

    // Release mutex
    mutex.unlock();

    return 0;
}
