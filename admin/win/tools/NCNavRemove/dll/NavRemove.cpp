/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <windows.h>
#include "utility.h"
#include "NavRemove.h"
#include "../ConfigIni.h"

using namespace NCTools;

extern bool g_alreadyRunning;

HRESULT NAVREMOVE_API RemoveNavigationPaneEntries()
{
    if (g_alreadyRunning) {
        return S_OK;
    }

    // Config
    ConfigIni ini;

    if (!ini.load()) {
        return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
    }

    Utility::removeNavigationPaneEntries(ini.getAppName());

    return S_OK;
}
