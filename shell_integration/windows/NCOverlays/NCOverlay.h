/*
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-FileCopyrightText: 2000-2013 Liferay, Inc. All rights reserved
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef NCOVERLAY_H
#define NCOVERLAY_H

#pragma once

#include <shlobj.h>
#include <fstream>

class NCOverlay : public IShellIconOverlayIdentifier

{
public:
    explicit NCOverlay(int state);

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
    std::ofstream m_logger;
};

#endif
