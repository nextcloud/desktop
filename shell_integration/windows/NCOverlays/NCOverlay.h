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

#ifndef NCOVERLAY_H
#define NCOVERLAY_H

#pragma once

#include <shlobj.h>

class NCOverlay : public IShellIconOverlayIdentifier

{
public:
    NCOverlay(int state);

    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP GetOverlayInfo(PWSTR pwszIconFile, int cchMax, int *pIndex, DWORD *pdwFlags);
    IFACEMETHODIMP GetPriority(int *pPriority);
    IFACEMETHODIMP IsMemberOf(PCWSTR pwszPath, DWORD dwAttrib);
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
    IFACEMETHODIMP_(ULONG) Release();

protected:
    ~NCOverlay();

private:
    long _referenceCount;
    int _state;
};

#endif