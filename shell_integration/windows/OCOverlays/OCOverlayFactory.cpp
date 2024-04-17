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

#include <windows.h>
#include <new>

#include "OCOverlayFactory.h"
#include "OCOverlay.h"

extern long dllReferenceCount;

OCOverlayFactory::OCOverlayFactory(int state)
    : _referenceCount(1), _state(state)
{
    InterlockedIncrement(&dllReferenceCount);
}

OCOverlayFactory::~OCOverlayFactory()
{
    InterlockedDecrement(&dllReferenceCount);
}

IFACEMETHODIMP OCOverlayFactory::QueryInterface(REFIID riid, void **ppv)
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

IFACEMETHODIMP_(ULONG) OCOverlayFactory::AddRef()
{
    return InterlockedIncrement(&_referenceCount);
}

IFACEMETHODIMP_(ULONG) OCOverlayFactory::Release()
{
    ULONG cRef = InterlockedDecrement(&_referenceCount);

    if (0 == cRef)
    {
        delete this;
    }
    return cRef;
}

IFACEMETHODIMP OCOverlayFactory::CreateInstance(
    IUnknown *pUnkOuter, REFIID riid, void **ppv)
{
    HRESULT hResult = CLASS_E_NOAGGREGATION;

    if (pUnkOuter != nullptr) {
        return hResult;
    }

    hResult = E_OUTOFMEMORY;
    OCOverlay *lrOverlay = new (std::nothrow) OCOverlay(_state);
    if (!lrOverlay) { return hResult; }

    hResult = lrOverlay->QueryInterface(riid, ppv);
    lrOverlay->Release();

    return hResult;
}

IFACEMETHODIMP OCOverlayFactory::LockServer(BOOL fLock)
{
    if (fLock) {
        InterlockedIncrement(&dllReferenceCount);
    } else {
        InterlockedDecrement(&dllReferenceCount);
    }
    return S_OK;
}
