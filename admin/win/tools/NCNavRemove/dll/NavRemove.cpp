/*
 * Copyright (C) by Michael Schuster <michael@schuster.ms>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
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
