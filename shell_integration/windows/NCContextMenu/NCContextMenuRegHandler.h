/*
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef NCCONTEXTMENUREGHANDLER_H
#define NCCONTEXTMENUREGHANDLER_H

#pragma once

#include <windows.h>

class __declspec(dllexport) NCContextMenuRegHandler
{
public:
    static HRESULT MakeRegistryEntries(const CLSID& clsid, PCWSTR fileType);
    static HRESULT RegisterCOMObject(PCWSTR modulePath, PCWSTR friendlyName, const CLSID& clsid);
    static HRESULT RemoveRegistryEntries(PCWSTR friendlyName);
    static HRESULT UnregisterCOMObject(const CLSID& clsid);

    static HRESULT RegisterInprocServer(PCWSTR pszModule, const CLSID& clsid, PCWSTR pszFriendlyName, PCWSTR pszThreadModel);
    static HRESULT UnregisterInprocServer(const CLSID& clsid);

    static HRESULT RegisterShellExtContextMenuHandler(PCWSTR pszFileType, const CLSID& clsid, PCWSTR pszFriendlyName);
    static HRESULT UnregisterShellExtContextMenuHandler(PCWSTR pszFileType, PCWSTR pszFriendlyName);
};

#endif //NCCONTEXTMENUREGHANDLER_H