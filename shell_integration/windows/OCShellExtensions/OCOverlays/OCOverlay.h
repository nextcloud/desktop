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

#ifndef OCOVERLAY_H
#define OCOVERLAY_H

#include "stdafx.h"

#pragma once

class RemotePathChecker;

class OCOverlay : public IShellIconOverlayIdentifier

{
public:
	OCOverlay(int state);

	IFACEMETHODIMP_(ULONG) AddRef();
	IFACEMETHODIMP GetOverlayInfo(PWSTR pwszIconFile, int cchMax, int *pIndex, DWORD *pdwFlags);
	IFACEMETHODIMP GetPriority(int *pPriority);
	IFACEMETHODIMP IsMemberOf(PCWSTR pwszPath, DWORD dwAttrib);
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
    IFACEMETHODIMP_(ULONG) Release();

protected:
    ~OCOverlay();

private:
	//bool _GenerateMessage(const wchar_t*, std::wstring*);

	bool _IsOverlaysEnabled();
    long _referenceCount;
	RemotePathChecker* _checker;
	int _state;
};

#endif