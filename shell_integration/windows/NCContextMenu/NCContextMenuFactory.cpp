/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "NCContextMenuFactory.h"
#include "NCContextMenu.h"
#include <new>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")


extern long g_cDllRef;


NCContextMenuFactory::NCContextMenuFactory() : m_cRef(1)
{
    InterlockedIncrement(&g_cDllRef);
}

NCContextMenuFactory::~NCContextMenuFactory()
{
    InterlockedDecrement(&g_cDllRef);
}


// IUnknown methods

IFACEMETHODIMP NCContextMenuFactory::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[] =  { QITABENT(NCContextMenuFactory, IClassFactory), { nullptr }, };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) NCContextMenuFactory::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) NCContextMenuFactory::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (0 == cRef) {
        delete this;
    }
    return cRef;
}


// IClassFactory methods

IFACEMETHODIMP NCContextMenuFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv)
{
    HRESULT hr = CLASS_E_NOAGGREGATION;

    // pUnkOuter is used for aggregation. We do not support it in the sample.
    if (!pUnkOuter) {
        hr = E_OUTOFMEMORY;

        // Create the COM component.
        auto pExt = new (std::nothrow) NCContextMenu();
        if (pExt) {
            // Query the specified interface.
            hr = pExt->QueryInterface(riid, ppv);
            pExt->Release();
        }
    }

    return hr;
}

IFACEMETHODIMP NCContextMenuFactory::LockServer(BOOL fLock)
{
    if (fLock)  {
        InterlockedIncrement(&g_cDllRef);
    } else {
        InterlockedDecrement(&g_cDllRef);
    }
    return S_OK;
}
