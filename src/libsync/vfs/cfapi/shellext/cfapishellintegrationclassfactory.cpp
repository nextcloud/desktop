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
#include <new>

extern long dllReferenceCount;

namespace VfsShellExtensions {

HRESULT CfApiShellIntegrationClassFactory::CreateInstance(
    REFCLSID clsid, const ClassObjectInit *classObjectInits, size_t classObjectInitsCount, REFIID riid, void **ppv)
{
    for (size_t i = 0; i < classObjectInitsCount; ++i) {
        if (clsid == *classObjectInits[i].clsid) {
            IClassFactory *classFactory =
                new (std::nothrow) CfApiShellIntegrationClassFactory(classObjectInits[i].pfnCreate);
            if (!classFactory) {
                return E_OUTOFMEMORY;
            }
            const auto hresult = classFactory->QueryInterface(riid, ppv);
            classFactory->Release();
            return hresult;
        }
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

// IUnknown
IFACEMETHODIMP CfApiShellIntegrationClassFactory::QueryInterface(REFIID riid, void **ppv)
{
    *ppv = nullptr;

    if (IsEqualIID(IID_IUnknown, riid) || IsEqualIID(IID_IClassFactory, riid)) {
        *ppv = static_cast<IUnknown *>(this);
        AddRef();
        return S_OK;
    } else {
        return E_NOINTERFACE;
    }
}

IFACEMETHODIMP_(ULONG) CfApiShellIntegrationClassFactory::AddRef()
{
    return InterlockedIncrement(&_referenceCount);
}

IFACEMETHODIMP_(ULONG) CfApiShellIntegrationClassFactory::Release()
{
    const auto refCount = InterlockedDecrement(&_referenceCount);
    if (refCount == 0) {
        delete this;
    }
    return refCount;
}

IFACEMETHODIMP CfApiShellIntegrationClassFactory::CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv)
{
    if (punkOuter) {
        return CLASS_E_NOAGGREGATION;
    }
    return _pfnCreate(riid, ppv);
}

IFACEMETHODIMP CfApiShellIntegrationClassFactory::LockServer(BOOL fLock)
{
    if (fLock) {
        InterlockedIncrement(&dllReferenceCount);
    } else {
        InterlockedDecrement(&dllReferenceCount);
    }
    return S_OK;
}

CfApiShellIntegrationClassFactory::CfApiShellIntegrationClassFactory(PFNCREATEINSTANCE pfnCreate)
    : _referenceCount(1)
    , _pfnCreate(pfnCreate)
{
    InterlockedIncrement(&dllReferenceCount);
}

CfApiShellIntegrationClassFactory::~CfApiShellIntegrationClassFactory()
{
    InterlockedDecrement(&dllReferenceCount);
}
}
