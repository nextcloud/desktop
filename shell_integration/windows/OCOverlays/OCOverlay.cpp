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

#include "OCOverlay.h"

#include "OCOverlayFactory.h"
#include "RegistryUtil.h"
#include "StringUtil.h"

#include "UtilConstants.h"
#include "RemotePathChecker.h"

#include "resource.h"

#include <algorithm>
#include <iostream>
#include <fstream>

using namespace std;

#pragma comment(lib, "shlwapi.lib")

extern HINSTANCE instanceHandle;

#define IDM_DISPLAY 0  
#define IDB_OK 101

OCOverlay::OCOverlay(int state) 
	: _referenceCount(1)
	, _state(state)

{
    static RemotePathChecker s_remotePathChecker;
    _checker = &s_remotePathChecker;
}

OCOverlay::~OCOverlay(void)
{
}

IFACEMETHODIMP_(ULONG) OCOverlay::AddRef()
{
    return InterlockedIncrement(&_referenceCount);
}

IFACEMETHODIMP OCOverlay::QueryInterface(REFIID riid, void **ppv)
{
    HRESULT hr = S_OK;

    if (IsEqualIID(IID_IUnknown, riid) ||  IsEqualIID(IID_IShellIconOverlayIdentifier, riid))
    {
        *ppv = static_cast<IShellIconOverlayIdentifier *>(this);
    }
    else
    {
        hr = E_NOINTERFACE;
        *ppv = NULL;
    }

    if (*ppv)
    {
        AddRef();
    }
	
    return hr;
}

IFACEMETHODIMP_(ULONG) OCOverlay::Release()
{
    ULONG cRef = InterlockedDecrement(&_referenceCount);
    if (0 == cRef)
    {
        delete this;
    }

    return cRef;
}

IFACEMETHODIMP OCOverlay::GetPriority(int *pPriority)
{
	// this defines which handler has prededence, so
	// we order this in terms of likelyhood
	switch (_state) {
	case State_OK:
		*pPriority = 0;
	case State_OKShared:
		*pPriority = 1;
	case State_Warning:
		*pPriority = 2;
	case State_WarningShared:
		*pPriority = 3;
	case State_Sync:
		*pPriority = 4;
	case State_SyncShared:
		*pPriority = 5;
	case State_Error:
		*pPriority = 6;
	case State_ErrorShared:
		*pPriority = 7;
	default:
		*pPriority = 8;
	}

	return S_OK;
}

 IFACEMETHODIMP OCOverlay::IsMemberOf(PCWSTR pwszPath, DWORD dwAttrib)
{
    auto watchedDirectories = _checker->WatchedDirectories();

	wstring wpath(pwszPath);
	//wpath.append(L"\\");
	vector<wstring>::iterator it;
	bool watched = false;
	for (it = watchedDirectories.begin(); it != watchedDirectories.end(); ++it) {
		if (StringUtil::begins_with(wpath, *it)) {
			watched = true;
		}
	}

	if (!watched) {
		return MAKE_HRESULT(S_FALSE, 0, 0);
	}

	int state = 0;
	if (!_checker->IsMonitoredPath(pwszPath, &state)) {
		return MAKE_HRESULT(S_FALSE, 0, 0);
	}
	return MAKE_HRESULT(state == _state ? S_OK : S_FALSE, 0, 0);
}

IFACEMETHODIMP OCOverlay::GetOverlayInfo(PWSTR pwszIconFile, int cchMax, int *pIndex, DWORD *pdwFlags)
{
	*pIndex = 0;
	*pdwFlags = ISIOI_ICONFILE | ISIOI_ICONINDEX;
	*pIndex = _state;

	if (GetModuleFileName(instanceHandle, pwszIconFile, cchMax) == 0) {	
		HRESULT hResult = HRESULT_FROM_WIN32(GetLastError());
		wcerr << L"IsOK? " << (hResult == S_OK) << L" with path " << pwszIconFile << L", index " << *pIndex << endl;
		return hResult;
	}

	return S_OK;
}


bool OCOverlay::_IsOverlaysEnabled()
{
	//int enable;
	bool success = false;
	
	//if(RegistryUtil::ReadRegistry(REGISTRY_ROOT_KEY, REGISTRY_ENABLE_OVERLAY, &enable))
	//{
	//	if(enable) {
	//		success = true;
	//	}
	//}

	return success;
}

