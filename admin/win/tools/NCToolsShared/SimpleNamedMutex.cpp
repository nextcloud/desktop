/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SimpleNamedMutex.h"

SimpleNamedMutex::SimpleNamedMutex(const std::wstring &name)
{
    _name = name;
}

bool SimpleNamedMutex::lock()
{
    if (_name.empty() || _hMutex) {
        return false;
    }

    // Mutex
    _hMutex = CreateMutex(nullptr, TRUE, _name.data());

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(_hMutex);
        _hMutex = nullptr;
        return false;
    }

    return true;
}

void SimpleNamedMutex::unlock()
{
    // Release mutex
    if (_hMutex) {
        ReleaseMutex(_hMutex);
        CloseHandle(_hMutex);
        _hMutex = nullptr;
    }
}
