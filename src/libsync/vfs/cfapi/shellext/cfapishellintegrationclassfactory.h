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
