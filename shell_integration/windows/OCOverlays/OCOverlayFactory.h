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

#ifndef OCOVERLAYFACTORY_H
#define OCOVERLAYFACTORY_H

#pragma once

enum State {
	State_Error = 0, State_ErrorShared,
	State_OK, State_OKShared,
	State_Sync, State_SyncShared,
	State_Warning, State_WarningShared
};

class OCOverlayFactory : public IClassFactory
{
public:
	OCOverlayFactory(int state);

	IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv);
	IFACEMETHODIMP LockServer(BOOL fLock);
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
    IFACEMETHODIMP_(ULONG) Release();

protected:
    ~OCOverlayFactory();

private:
    long _referenceCount;
	int _state;
};

#endif