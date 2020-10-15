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
