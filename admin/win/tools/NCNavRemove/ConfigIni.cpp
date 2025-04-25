/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <windows.h>
#include "3rdparty/SimpleIni.h"
#include "NavRemoveConstants.h"
#include "ConfigIni.h"

ConfigIni::ConfigIni()
{
}

bool ConfigIni::load()
{
    const DWORD bufferLen = GetCurrentDirectory(0, nullptr);
    TCHAR *pszBuffer = nullptr;

    if (bufferLen == 0) {
        return false;
    }

    pszBuffer = new TCHAR[bufferLen];
    if (!pszBuffer) {
        return false;
    }

    std::wstring filename;
    if (GetCurrentDirectory(bufferLen, pszBuffer) != 0) {
        filename = pszBuffer;
    }
    delete[] pszBuffer;

    if (filename.empty()) {
        return false;
    }

    filename.append(L"\\");
    filename.append(INI_NAME);

    CSimpleIni ini;
    const wchar_t iniSection[] = CFG_KEY;
    const wchar_t iniKey[] = CFG_VAR_APPNAME;

    const auto rc = ini.LoadFile(filename.data());

    if (rc != SI_OK) {
        return false;
    }

    const auto pv = ini.GetValue(iniSection, iniKey);
    bool success = false;

    if (pv) {
        _appName = pv;
        success = !_appName.empty();
    }
 
    ini.Reset();

    return success;
}

std::wstring ConfigIni::getAppName() const
{
    return _appName;
}
