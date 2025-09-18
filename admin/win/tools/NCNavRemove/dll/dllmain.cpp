/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
