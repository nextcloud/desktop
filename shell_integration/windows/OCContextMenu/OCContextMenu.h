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

#ifndef OCCONTEXTMENU_H
#define OCCONTEXTMENU_H

#pragma once
#include <shlobj.h>     // For IShellExtInit and IContextMenu

class OCContextMenu : public IShellExtInit, public IContextMenu
{
public:
	// IUnknown
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
	IFACEMETHODIMP_(ULONG) AddRef();
	IFACEMETHODIMP_(ULONG) Release();

	// IShellExtInit
	IFACEMETHODIMP Initialize(LPCITEMIDLIST pidlFolder, LPDATAOBJECT pDataObj, HKEY hKeyProgID);

	// IContextMenu
	IFACEMETHODIMP QueryContextMenu(HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags);
	IFACEMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO pici);
	IFACEMETHODIMP GetCommandString(UINT_PTR idCommand, UINT uFlags, UINT *pwReserved, LPSTR pszName, UINT cchMax);

	OCContextMenu();

protected:
	~OCContextMenu();

private:
	// Reference count of component.
	long m_cRef;

	// The name of the selected file.
	wchar_t m_szSelectedFile[MAX_PATH];

    // The method that handles the "ocshare" verb.
    void OnVerbShare(HWND hWnd);
    void OnVerbCopyLink(HWND hWnd);
    void OnVerbEmailLink(HWND hWnd);

	PWSTR m_pszMenuText;
	PCSTR m_pszVerb;
	PCWSTR m_pwszVerb;
	PCSTR m_pszVerbCanonicalName;
	PCWSTR m_pwszVerbCanonicalName;
	PCSTR m_pszVerbHelpText;
	PCWSTR m_pwszVerbHelpText;
};
	
#endif //OCCONTEXTMENU_H
