/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "cfapishellintegrationclassfactory.h"
#include "customstateprovider.h"
#include "thumbnailprovider.h"
#include <comdef.h>

long dllReferenceCount = 0;
long dllObjectsCount = 0;

HINSTANCE instanceHandle = nullptr;

HRESULT CustomStateProvider_CreateInstance(REFIID riid, void **ppv);
HRESULT ThumbnailProvider_CreateInstance(REFIID riid, void **ppv);

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
