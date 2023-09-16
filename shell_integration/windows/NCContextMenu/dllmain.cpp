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

#include <windows.h>
#include <Guiddef.h>
#include "NCContextMenuRegHandler.h"
#include "NCContextMenuFactory.h"
#include "WinShellExtConstants.h"

HINSTANCE   g_hInst = nullptr;
long        g_cDllRef = 0;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        // Hold the instance of this DLL module, we will use it to get the 
        // path of the DLL to register the component.
        g_hInst = hModule;
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    HRESULT hr = 0;
    GUID guid;

    hr = CLSIDFromString(CONTEXT_MENU_GUID, (LPCLSID)&guid);
    if (!SUCCEEDED(hr)) {
        return hr;
    }

    hr = CLASS_E_CLASSNOTAVAILABLE;

    if (IsEqualCLSID(guid, rclsid))	{
        hr = E_OUTOFMEMORY;

        auto pClassFactory = new NCContextMenuFactory();
        if (pClassFactory) {
            hr = pClassFactory->QueryInterface(riid, ppv);
            pClassFactory->Release();
        }
    }

    return hr;
}

STDAPI DllCanUnloadNow(void)
{
    return g_cDllRef > 0 ? S_FALSE : S_OK;
}

STDAPI DllRegisterServer(void)
{
    HRESULT hr = 0;
    GUID guid;

    hr = CLSIDFromString(CONTEXT_MENU_GUID, (LPCLSID)&guid);
    if (!SUCCEEDED(hr)) {
        return hr;
    }

    wchar_t szModule[MAX_PATH];
    if (GetModuleFileName(g_hInst, szModule, ARRAYSIZE(szModule)) == 0)	{
        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    // Register the component.
    hr = NCContextMenuRegHandler::RegisterInprocServer(szModule, guid,
        CONTEXT_MENU_DESCRIPTION, L"Apartment");
    if (SUCCEEDED(hr))	{
        // Register the context menu handler. The context menu handler is 
        // associated with the .cpp file class.
        hr = NCContextMenuRegHandler::RegisterShellExtContextMenuHandler(L"AllFileSystemObjects", guid, CONTEXT_MENU_REGKEY_NAME);
    }

    return hr;
}

STDAPI DllUnregisterServer(void)
{
    HRESULT hr = S_OK;
    GUID guid;

    hr = CLSIDFromString(CONTEXT_MENU_GUID, (LPCLSID)&guid);
    if (!SUCCEEDED(hr)) {
        return hr;
    }

    wchar_t szModule[MAX_PATH];
    if (GetModuleFileName(g_hInst, szModule, ARRAYSIZE(szModule)) == 0)	{
        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    // Unregister the component.
    hr = NCContextMenuRegHandler::UnregisterInprocServer(guid);
    if (SUCCEEDED(hr))	{
        // Unregister the context menu handler.
        hr = NCContextMenuRegHandler::UnregisterShellExtContextMenuHandler(L"AllFileSystemObjects", CONTEXT_MENU_REGKEY_NAME);
    }

    return hr;
}
