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
