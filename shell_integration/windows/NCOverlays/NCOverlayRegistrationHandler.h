/*
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-FileCopyrightText: 2000-2013 Liferay, Inc. All rights reserved
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef NCOVERLAYREGISTRATIONHANDLER_H
#define NCOVERLAYREGISTRATIONHANDLER_H

#pragma once

#include <windows.h>

class __declspec(dllexport) NCOverlayRegistrationHandler 
{
    public:
        static HRESULT MakeRegistryEntries(const CLSID& clsid, PCWSTR fileType);
        static HRESULT RegisterCOMObject(PCWSTR modulePath, PCWSTR friendlyName, const CLSID& clsid);
        static HRESULT RemoveRegistryEntries(PCWSTR friendlyName);
        static HRESULT UnregisterCOMObject(const CLSID& clsid);
};

#endif