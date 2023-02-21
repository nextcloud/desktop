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

#include "NCOverlayRegistrationHandler.h"

#include <windows.h>
#include <objbase.h>
#include <iostream>
#include <fstream>

#define REGISTRY_OVERLAY_KEY        LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers)"
#define REGISTRY_CLSID              L"CLSID"
#define REGISTRY_IN_PROCESS         L"InprocServer32"
#define REGISTRY_THREADING          L"ThreadingModel"
#define REGISTRY_APARTMENT          L"Apartment"
#define REGISTRY_VERSION            L"Version"
#define REGISTRY_VERSION_NUMBER     L"1.0"

using namespace std;

HRESULT NCOverlayRegistrationHandler::MakeRegistryEntries(const CLSID& clsid, PCWSTR friendlyName)
{
    HRESULT hResult = 0;
    HKEY shellOverlayKey = nullptr;
    // the key may not exist yet
    hResult = HRESULT_FROM_WIN32(RegCreateKeyEx(HKEY_LOCAL_MACHINE, REGISTRY_OVERLAY_KEY, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &shellOverlayKey, nullptr));
    if (!SUCCEEDED(hResult)) {
        hResult = RegCreateKey(HKEY_LOCAL_MACHINE, REGISTRY_OVERLAY_KEY, &shellOverlayKey);
        if(!SUCCEEDED(hResult)) {
            return hResult;
        }
    }

    HKEY syncExOverlayKey = nullptr;
    hResult = HRESULT_FROM_WIN32(RegCreateKeyEx(shellOverlayKey, friendlyName, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &syncExOverlayKey, nullptr));

    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    wchar_t stringCLSID[MAX_PATH];
    StringFromGUID2(clsid, stringCLSID, ARRAYSIZE(stringCLSID));
    LPCTSTR value = stringCLSID;
    hResult = RegSetValueEx(syncExOverlayKey, nullptr, 0, REG_SZ, (LPBYTE)value, (DWORD)((wcslen(value)+1) * sizeof(TCHAR)));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    return hResult;
}

HRESULT NCOverlayRegistrationHandler::RemoveRegistryEntries(PCWSTR friendlyName)
{
    HRESULT hResult = 0;
    HKEY shellOverlayKey = nullptr;
    hResult = HRESULT_FROM_WIN32(RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGISTRY_OVERLAY_KEY, 0, KEY_WRITE, &shellOverlayKey));

    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    HKEY syncExOverlayKey = nullptr;
    hResult = HRESULT_FROM_WIN32(RegDeleteKey(shellOverlayKey, friendlyName));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    return hResult;
}

HRESULT NCOverlayRegistrationHandler::RegisterCOMObject(PCWSTR modulePath, PCWSTR friendlyName, const CLSID& clsid)
{
    if (!modulePath) {
        return E_FAIL;
    }

    wchar_t stringCLSID[MAX_PATH];
    StringFromGUID2(clsid, stringCLSID, ARRAYSIZE(stringCLSID));
    HRESULT hResult = 0;
    HKEY hKey = nullptr;

    hResult = HRESULT_FROM_WIN32(RegOpenKeyEx(HKEY_CLASSES_ROOT, REGISTRY_CLSID, 0, KEY_WRITE, &hKey));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    HKEY clsidKey = nullptr;
    hResult = HRESULT_FROM_WIN32(RegCreateKeyEx(hKey, stringCLSID, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &clsidKey, nullptr));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    hResult = HRESULT_FROM_WIN32(RegSetValue(clsidKey, nullptr, REG_SZ, friendlyName, (DWORD) wcslen(friendlyName)));

    HKEY inprocessKey = nullptr;
    hResult = HRESULT_FROM_WIN32(RegCreateKeyEx(clsidKey, REGISTRY_IN_PROCESS, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &inprocessKey, nullptr));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    hResult = HRESULT_FROM_WIN32(RegSetValue(inprocessKey, nullptr, REG_SZ, modulePath, (DWORD) wcslen(modulePath)));

    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    hResult = HRESULT_FROM_WIN32(RegSetValueEx(inprocessKey, REGISTRY_THREADING, 0, REG_SZ, (LPBYTE)REGISTRY_APARTMENT, (DWORD)((wcslen(REGISTRY_APARTMENT)+1) * sizeof(TCHAR))));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    HKEY versionKey = nullptr;
    hResult = HRESULT_FROM_WIN32(RegCreateKeyEx(clsidKey, REGISTRY_VERSION, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &versionKey, nullptr));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    hResult = HRESULT_FROM_WIN32(RegSetValueEx(versionKey, nullptr, 0, REG_SZ, (LPBYTE)REGISTRY_VERSION_NUMBER, (DWORD)(wcslen(REGISTRY_VERSION_NUMBER)+1) * sizeof(TCHAR)));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    return S_OK;
}

HRESULT NCOverlayRegistrationHandler::UnregisterCOMObject(const CLSID& clsid)
{
    wchar_t stringCLSID[MAX_PATH];

    StringFromGUID2(clsid, stringCLSID, ARRAYSIZE(stringCLSID));
    HRESULT hResult = 0;
    HKEY hKey = nullptr;
    hResult = HRESULT_FROM_WIN32(RegOpenKeyEx(HKEY_CLASSES_ROOT, REGISTRY_CLSID, 0, DELETE, &hKey));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    HKEY clsidKey = nullptr;
    hResult = HRESULT_FROM_WIN32(RegOpenKeyEx(hKey, stringCLSID, 0, DELETE, &clsidKey));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    hResult = HRESULT_FROM_WIN32(RegDeleteKey(clsidKey, REGISTRY_IN_PROCESS));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    hResult = HRESULT_FROM_WIN32(RegDeleteKey(clsidKey, REGISTRY_VERSION));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    hResult = HRESULT_FROM_WIN32(RegDeleteKey(hKey, stringCLSID));
    if (!SUCCEEDED(hResult)) {
        return hResult;
    }

    return S_OK;
}
