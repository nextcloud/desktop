/*
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef NCCONTEXTMENU_H
#define NCCONTEXTMENU_H

#pragma once
#include "NCClientInterface.h"

#include <shlobj.h>     // For IShellExtInit and IContextMenu

#include <string>
#include <fstream>
#include <iostream>

class NCContextMenu : public IShellExtInit, public IContextMenu
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

	NCContextMenu();

protected:
	~NCContextMenu();

private:
	// Reference count of component.
	long m_cRef;

	// The name of the selected files (separated by '\x1e')
	std::wstring m_selectedFiles;
	NCClientInterface::ContextMenuInfo m_info;

    std::ofstream m_logger;
};
	
#endif //NCCONTEXTMENU_H
