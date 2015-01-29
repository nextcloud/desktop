/**
* Copyright (c) 2015 Daniel Molkentin <danimo@owncloud.com>. All rights reserved.
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


#ifndef OCCONTEXTMENUFACTORY_H
#define OCCONTEXTMENUFACTORY_H

#pragma once

#include <unknwn.h>     // For IClassFactory

class OCContextMenuFactory : public IClassFactory
{
public:
	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
	IFACEMETHODIMP_(ULONG) AddRef();
	IFACEMETHODIMP_(ULONG) Release();

	// IClassFactory
	IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv);
	IFACEMETHODIMP LockServer(BOOL fLock);

	OCContextMenuFactory();

private:
	~OCContextMenuFactory();
	long m_cRef;
};

#endif //OCCONTEXTMENUFACTORY_H