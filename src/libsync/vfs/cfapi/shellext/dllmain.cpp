/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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

#include "cfapishellintegrationclassfactory.h"
#include "customstateprovider.h"
#include "thumbnailprovider.h"
#include <comdef.h>
#include <tchar.h>

long dllReferenceCount = 0;
long dllObjectsCount = 0;

HINSTANCE instanceHandle = nullptr;

HRESULT CustomStateProvider_CreateInstance(REFIID riid, void **ppv);
HRESULT ThumbnailProvider_CreateInstance(REFIID riid, void **ppv);

HWND hHiddenWnd = nullptr;
DWORD WINAPI MessageLoopThread(LPVOID lpParameter);
LRESULT CALLBACK HiddenWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void CreateHiddenWindowAndLaunchMessageLoop();

const VfsShellExtensions::ClassObjectInit listClassesSupported[] = {
    {&__uuidof(winrt::CfApiShellExtensions::implementation::CustomStateProvider), CustomStateProvider_CreateInstance},
    {&__uuidof(VfsShellExtensions::ThumbnailProvider), ThumbnailProvider_CreateInstance}
};

STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, void *)
{
    if (dwReason == DLL_PROCESS_ATTACH) {
        instanceHandle = hInstance;
        wchar_t dllFilePath[_MAX_PATH] = {0};
        ::GetModuleFileName(instanceHandle, dllFilePath, _MAX_PATH);
        winrt::CfApiShellExtensions::implementation::CustomStateProvider::setDllFilePath(dllFilePath);
        DisableThreadLibraryCalls(hInstance);
  
        CreateHiddenWindowAndLaunchMessageLoop();
    }

    return TRUE;
}

STDAPI DllCanUnloadNow()
{
    return (dllReferenceCount == 0 && dllObjectsCount == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **ppv)
{
    return VfsShellExtensions::CfApiShellIntegrationClassFactory::CreateInstance(clsid, listClassesSupported, ARRAYSIZE(listClassesSupported), riid, ppv);
}

HRESULT CustomStateProvider_CreateInstance(REFIID riid, void **ppv)
{
    try {
        const auto customStateProvider = winrt::make_self<winrt::CfApiShellExtensions::implementation::CustomStateProvider>();
        return customStateProvider->QueryInterface(riid, ppv);
    } catch (_com_error exc) {
        return exc.Error();
    }
}

HRESULT ThumbnailProvider_CreateInstance(REFIID riid, void **ppv)
{
    auto *thumbnailProvider = new (std::nothrow) VfsShellExtensions::ThumbnailProvider();
    if (!thumbnailProvider) {
        return E_OUTOFMEMORY;
    }
    const auto hresult = thumbnailProvider->QueryInterface(riid, ppv);
    thumbnailProvider->Release();
    return hresult;
}

void CreateHiddenWindowAndLaunchMessageLoop()
{
    const WNDCLASSEX hiddenWindowClass {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        HiddenWndProc,
        0L,
        0L,
        GetModuleHandle(NULL),
        NULL,
        NULL,
        NULL,
        NULL,
        _T(CFAPI_SHELLEXT_WINDOW_CLASS_NAME),
        NULL
    };

    RegisterClassEx(&hiddenWindowClass);

    hHiddenWnd = CreateWindow(
        hiddenWindowClass.lpszClassName,
        _T(""),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        NULL,
        NULL,
        hiddenWindowClass.hInstance,
        NULL);

    ShowWindow(hHiddenWnd, SW_HIDE);
    UpdateWindow(hHiddenWnd);

    const auto hMessageLoopThread = CreateThread(NULL, 0, MessageLoopThread, NULL, 0, NULL);
    if (hMessageLoopThread) {
        CloseHandle(hMessageLoopThread);
    }
}

DWORD WINAPI MessageLoopThread(LPVOID lpParameter)
{
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

LRESULT CALLBACK HiddenWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CLOSE:
        FreeLibrary(instanceHandle);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
