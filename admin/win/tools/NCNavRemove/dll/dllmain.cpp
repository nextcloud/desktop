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
#include "SimpleNamedMutex.h"
#include "NavRemoveConstants.h"

SimpleNamedMutex g_mutex(std::wstring(MUTEX_NAME));
bool g_alreadyRunning = false;

extern "C" BOOL WINAPI DllMain(
    __in HINSTANCE hInst,
    __in ULONG ulReason,
    __in LPVOID
    )
{
    switch(ulReason)
    {
    case DLL_PROCESS_ATTACH:
        // Mutex
        g_alreadyRunning = !g_mutex.lock();
        break;

    case DLL_PROCESS_DETACH:
        // Release mutex
        g_mutex.unlock();
        break;
    }

    return TRUE;
}
