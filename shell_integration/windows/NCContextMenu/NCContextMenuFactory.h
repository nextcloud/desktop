/*
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef NCCONTEXTMENUFACTORY_H
#define NCCONTEXTMENUFACTORY_H

#pragma once

#include <unknwn.h>     // For IClassFactory

class NCContextMenuFactory : public IClassFactory
{
public:
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv);
    IFACEMETHODIMP LockServer(BOOL fLock);

    NCContextMenuFactory();

private:
    ~NCContextMenuFactory();
    long m_cRef;
};

#endif //NCCONTEXTMENUFACTORY_H