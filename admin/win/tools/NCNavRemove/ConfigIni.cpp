/*
 * Copyright (C) by Michael Schuster <michael.schuster@nextcloud.com>
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

#include "NCTools.h"
#include "3rdparty/SimpleIni.h"
#include "NavRemoveConstants.h"
#include "ConfigIni.h"

ConfigIni::ConfigIni()
{
}

bool ConfigIni::load()
{
    std::wstring filename;
    DWORD bufferLen = GetCurrentDirectory(0, nullptr);
    TCHAR *pszBuffer = nullptr;

    if (bufferLen == 0) {
        return false;
    }

    pszBuffer = new TCHAR[bufferLen];
    if (!pszBuffer) {
        return false;
    }

    if (GetCurrentDirectory(bufferLen, pszBuffer) != 0) {
        filename = pszBuffer;
    }
    delete [] pszBuffer;

    if (filename.empty()) {
        return false;
    }

    filename.append(L"\\");
    filename.append(INI_NAME);

    CSimpleIni ini;
    const wchar_t iniSection[] = CFG_KEY;
    const wchar_t iniKey[] = CFG_VAR_APPNAME;

    auto rc = ini.LoadFile(filename.data());

    if (rc != SI_OK) {
        return false;
    }

    auto pv = ini.GetValue(iniSection, iniKey);
    bool success = false;

    if (pv) {
        _appName = pv;
        
        success = !_appName.empty();
    }
 
    ini.Reset();

    return success;
}

const std::wstring ConfigIni::getAppName() const
{
    return _appName;
}
