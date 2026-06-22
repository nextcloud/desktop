/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-FileCopyrightText: 2000-2013 Liferay, Inc. All rights reserved
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <windows.h>
#include <new>

#include "NCOverlayFactory.h"
#include "NCOverlay.h"

extern long dllReferenceCount;

NCOverlayFactory::NCOverlayFactory(int state)
    : _referenceCount(1), _state(state)
{
    InterlockedIncrement(&dllReferenceCount);
}

NCOverlayFactory::~NCOverlayFactory()
{
    InterlockedDecrement(&dllReferenceCount);
}

IFACEMETHODIMP NCOverlayFactory::QueryInterface(REFIID riid, void **ppv)
{
    HRESULT hResult = S_OK;

    if (IsEqualIID(IID_IUnknown, riid) || 
        IsEqualIID(IID_IClassFactory, riid))
    {
        *ppv = static_cast<IUnknown *>(this);
        AddRef();
    }
    else
    {
        hResult = E_NOINTERFACE;
        *ppv = nullptr;
    }

    return hResult;
}

IFACEMETHODIMP_(ULONG) NCOverlayFactory::AddRef()
{
    return InterlockedIncrement(&_referenceCount);
}

IFACEMETHODIMP_(ULONG) NCOverlayFactory::Release()
{
    ULONG cRef = InterlockedDecrement(&_referenceCount);

    if (0 == cRef)
    {
        delete this;
    }
    return cRef;
}

IFACEMETHODIMP NCOverlayFactory::CreateInstance(
    IUnknown *pUnkOuter, REFIID riid, void **ppv)
{
    HRESULT hResult = CLASS_E_NOAGGREGATION;

    if (pUnkOuter) { return hResult; }

    hResult = E_OUTOFMEMORY;
    auto lrOverlay = new (std::nothrow) NCOverlay(_state);
    if (!lrOverlay) { return hResult; }

    hResult = lrOverlay->QueryInterface(riid, ppv);
    lrOverlay->Release();

    return hResult;
}

IFACEMETHODIMP NCOverlayFactory::LockServer(BOOL fLock)
{
    if (fLock) {
        InterlockedIncrement(&dllReferenceCount);
    } else {
        InterlockedDecrement(&dllReferenceCount);
    }
    return S_OK;
}
