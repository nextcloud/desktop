/*
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-FileCopyrightText: 2000-2013 Liferay, Inc. All rights reserved
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef NCOVERLAYFACTORY_H
#define NCOVERLAYFACTORY_H

#pragma once

#include <unknwn.h>

enum State {
    State_Error = 0,
    State_OK, State_OKShared,
    State_Sync, 
    State_Warning
};

class NCOverlayFactory : public IClassFactory
{
public:
    NCOverlayFactory(int state);

    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv);
    IFACEMETHODIMP LockServer(BOOL fLock);
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
    IFACEMETHODIMP_(ULONG) Release();

protected:
    ~NCOverlayFactory();

private:
    long _referenceCount;
    int _state;
};

#endif