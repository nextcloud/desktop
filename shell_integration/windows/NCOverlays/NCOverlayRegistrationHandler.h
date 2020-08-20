/**
 * Copyright (c) 2000-2013 Liferay, Inc. All rights reserved.
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