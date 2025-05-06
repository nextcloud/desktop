/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <unknwn.h>

namespace VfsShellExtensions {

using PFNCREATEINSTANCE = HRESULT (*)(REFIID riid, void **ppvObject);
struct ClassObjectInit
{
    const CLSID *clsid;
    PFNCREATEINSTANCE pfnCreate;
};

class CfApiShellIntegrationClassFactory : public IClassFactory
{
public:
    CfApiShellIntegrationClassFactory(PFNCREATEINSTANCE pfnCreate);

    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv);

    static HRESULT CreateInstance(
        REFCLSID clsid, const ClassObjectInit *classObjectInits, size_t classObjectInitsCount, REFIID riid, void **ppv);

    IFACEMETHODIMP LockServer(BOOL fLock);
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
    IFACEMETHODIMP_(ULONG) Release();

protected:
    ~CfApiShellIntegrationClassFactory();

private:
    long _referenceCount;

    PFNCREATEINSTANCE _pfnCreate;
};
}
