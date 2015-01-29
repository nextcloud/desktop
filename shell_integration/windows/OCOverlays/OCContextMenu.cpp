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

#include "OCContextMenu.h"
#include "stdafx.h"

#define IDM_SHARE 0

OCContextMenu::OCContextMenu()
	: m_pwszVerb(0)
	, m_referenceCount(0)
{
}


OCContextMenu::~OCContextMenu()
{
}

IFACEMETHODIMP_(ULONG) OCContextMenu::AddRef()
{
	return InterlockedIncrement(&m_referenceCount);
}

IFACEMETHODIMP_(ULONG) OCContextMenu::Release()
{
	ULONG cRef = InterlockedDecrement(&m_referenceCount);
	if (0 == cRef)
	{
		delete this;
	}

	return cRef;
}

IFACEMETHODIMP OCContextMenu::QueryInterface(REFIID riid, void **ppv)
{
	HRESULT hr = S_OK;

	if (IsEqualIID(IID_IUnknown, riid) || IsEqualIID(IID_IContextMenu, riid))
	{
		*ppv = static_cast<IContextMenu *>(this);
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

IFACEMETHODIMP OCContextMenu::GetCommandString(UINT_PTR idCmd, UINT uFlags, UINT *pwReserved, LPSTR pszName, UINT cchMax)
{
	HRESULT hr = E_INVALIDARG;

	if (idCmd == IDM_SHARE)
	{
		switch (uFlags)
		{
		case GCS_HELPTEXTW:
			hr = StringCchCopyW(reinterpret_cast<PWSTR>(pszName), cchMax, L"Shares file or directory with ownCloud");
			break;

		case GCS_VERBW:
			// GCS_VERBW is an optional feature that enables a caller
			// to discover the canonical name for the verb that is passed in
			// through idCommand.
			hr = StringCchCopyW(reinterpret_cast<PWSTR>(pszName), cchMax, L"ownCloudShare");
			break;
		}
	}
	return hr;
}

IFACEMETHODIMP OCContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
	if (pici->cbSize == sizeof(CMINVOKECOMMANDINFOEX) &&
		(pici->fMask & CMIC_MASK_UNICODE))
	{
		return E_FAIL;
	}
	return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));
	if (HIWORD(((CMINVOKECOMMANDINFOEX *)pici)->lpVerbW))
	{
		if (StrCmpIW(((CMINVOKECOMMANDINFOEX *)pici)->lpVerbW, m_pwszVerb))
		{
			return E_FAIL;
		}
	}

	if (LOWORD(pici->lpVerb) != IDM_SHARE) {
		return E_FAIL;
	}

	MessageBox(pici->hwnd,
		L"ownCloud was here",
		L"ownCloud was here",
		MB_OK | MB_ICONINFORMATION);

}

IFACEMETHODIMP OCContextMenu::QueryContextMenu(HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
	HRESULT hr;

	if (!(CMF_DEFAULTONLY & uFlags))
	{
		InsertMenu(hMenu, indexMenu, MF_STRING | MF_BYPOSITION,	idCmdFirst + IDM_SHARE,	L"&Share with ownCloud");
	}
	hr = StringCbCopyW(m_pwszVerb, sizeof(m_pwszVerb), L"ownCloudShare");

	return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(IDM_SHARE + 1));
}

