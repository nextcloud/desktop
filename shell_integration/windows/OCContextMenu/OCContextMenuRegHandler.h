/**
* Copyright (c) 2015 Daniel Molkentin <danimo@owncloud.com>. All rights reserved.
*
* This library is free software; you can redistribute it and/or modify it under
* the terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; either version 2.1 of the License, or (at your option)
* any later version.
*
* This library is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
* details.
*/


#ifndef OCCONTEXTMENUREGHANDLER_H
#define OCCONTEXTMENUREGHANDLER_H

#pragma once

#include "stdafx.h"

class __declspec(dllexport) OCContextMenuRegHandler
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

#endif //OCCONTEXTMENUREGHANDLER_H