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

#include "stdafx.h"

#include "OCContextMenu.h"
#include "OCClientInterface.h"

#include <shobjidl.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <StringUtil.h>

extern long g_cDllRef;
enum {
	IDM_FIRST = 0,
	IDM_SHARE = IDM_FIRST,
	IDM_DRIVEMENU,
	IDM_DRIVEMENU_OFFLINE,
	IDM_DRIVEMENU_ONLINE,
	IDM_LAST
};

OCContextMenu::OCContextMenu(void) 
    : m_cRef(1)
{
    InterlockedIncrement(&g_cDllRef);
}

OCContextMenu::~OCContextMenu(void)
{
    InterlockedDecrement(&g_cDllRef);
}

#pragma region IUnknown

// Query to the interface the component supported.
IFACEMETHODIMP OCContextMenu::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(OCContextMenu, IContextMenu),
        QITABENT(OCContextMenu, IShellExtInit),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

// Increase the reference count for an interface on an object.
IFACEMETHODIMP_(ULONG) OCContextMenu::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

// Decrease the reference count for an interface on an object.
IFACEMETHODIMP_(ULONG) OCContextMenu::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (0 == cRef) {
        delete this;
    }

    return cRef;
}

#pragma endregion


#pragma region IShellExtInit

// Initialize the context menu handler.
IFACEMETHODIMP OCContextMenu::Initialize(
    LPCITEMIDLIST pidlFolder, LPDATAOBJECT pDataObj, HKEY hKeyProgID)
{
    m_selectedFiles.clear();

    if (!pDataObj) {
        return E_INVALIDARG;
    }

    FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stm;

    if (SUCCEEDED(pDataObj->GetData(&fe, &stm))) {
        // Get an HDROP handle.
        HDROP hDrop = static_cast<HDROP>(GlobalLock(stm.hGlobal));
        if (hDrop) {
            UINT nFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
            for (int i = 0; i < nFiles; ++i) {
                // Get the path of the file.
                wchar_t buffer[MAX_PATH];

                if (!DragQueryFile(hDrop, i, buffer, ARRAYSIZE(buffer))) {
                    m_selectedFiles.clear();
                    break;
                }

                if (i)
                    m_selectedFiles += L'\x1e';
                m_selectedFiles += buffer;
            }

            GlobalUnlock(stm.hGlobal);
        }

        ReleaseStgMedium(&stm);
    }

    // If any value other than S_OK is returned from the method, the context 
    // menu item is not displayed.
    return m_selectedFiles.empty() ? E_FAIL : S_OK;
}

#pragma endregion


#pragma region IContextMenu

void InsertSeperator(HMENU hMenu, UINT indexMenu)
{
    // Add a separator.
    MENUITEMINFO sep = { sizeof(sep) };
    sep.fMask = MIIM_TYPE;
    sep.fType = MFT_SEPARATOR;
    InsertMenuItem(hMenu, indexMenu, TRUE, &sep);
}

IFACEMETHODIMP OCContextMenu::QueryContextMenu(HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    // If uFlags include CMF_DEFAULTONLY then we should not do anything.
    if (CMF_DEFAULTONLY & uFlags)
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));
    }

    m_info = OCClientInterface::FetchInfo(m_selectedFiles);
    if (m_info.menuItems.empty()) {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));
    }

    InsertSeperator(hMenu, indexMenu++);

    HMENU hSubmenu = CreateMenu();
    {
        MENUITEMINFO mii = { sizeof(mii) };
        mii.fMask = MIIM_SUBMENU | MIIM_FTYPE | MIIM_STRING;
        mii.hSubMenu = hSubmenu;
        mii.fType = MFT_STRING;
        mii.dwTypeData = &m_info.contextMenuTitle[0];

        if (!InsertMenuItem(hMenu, indexMenu++, TRUE, &mii))
            return HRESULT_FROM_WIN32(GetLastError());
    }
    InsertSeperator(hMenu, indexMenu++);

    UINT indexSubMenu = 0;
    for (auto &item : m_info.menuItems) {
        bool disabled = item.flags.find(L'd') != std::string::npos;

        MENUITEMINFO mii = { sizeof(mii) };
        mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STRING | MIIM_STATE;
        mii.wID = idCmdFirst + indexSubMenu;
        mii.fType = MFT_STRING;
        mii.dwTypeData = &item.title[0];
        mii.fState = disabled ? MFS_DISABLED : MFS_ENABLED;

        
        if (!InsertMenuItem(hSubmenu, indexSubMenu, true, &mii))
            return HRESULT_FROM_WIN32(GetLastError());
        indexSubMenu++;
    }

	// Query the download mode 
	std::wstring downloadMode = OCClientInterface::GetDownloadMode(m_szSelectedFile);
	bool checkOnlineItem = downloadMode == L"ONLINE";
	bool checkOfflineItem = downloadMode == L"OFFLINE";

    // Insert the drive online|offline submenu
	{
		// Create the submenu
		HMENU hDriveSubMenu = CreateMenu();
		if (!hDriveSubMenu)
			return HRESULT_FROM_WIN32(GetLastError()); 

        // Setup the "Online" item
        MENUITEMINFO menuInfoDriveOnline {0};
        menuInfoDriveOnline.cbSize = sizeof (MENUITEMINFO);
        menuInfoDriveOnline.fMask = MIIM_STRING;
        menuInfoDriveOnline.dwTypeData = &m_info.streamOnlineItemTitle[0];
        menuInfoDriveOnline.fMask |= MIIM_ID;
        menuInfoDriveOnline.wID = idCmdFirst + IDM_DRIVEMENU_ONLINE; // qué demionios hace esta linea
        menuInfoDriveOnline.fMask |= MIIM_STATE;
        menuInfoDriveOnline.fState = MFS_ENABLED;
		if (checkOnlineItem)
			menuInfoDriveOnline.fState |= MFS_CHECKED;
        // Insert it into the submenu
        if(!InsertMenuItem(hDriveSubMenu, 
            0, // At position zero
            TRUE, //  indicates the existing item by using its zero-based position. (For example, the first item in the menu has a position of 0.) 
            &menuInfoDriveOnline
            ))
		{
			return HRESULT_FROM_WIN32(GetLastError());
		}


        // Setup the "Online" item
        MENUITEMINFO menuInfoDriveOffline {0};
        menuInfoDriveOffline.cbSize = sizeof (MENUITEMINFO);
        menuInfoDriveOffline.fMask = MIIM_STRING;
        menuInfoDriveOffline.dwTypeData = &m_info.streamOfflineItemTitle[0];
        menuInfoDriveOffline.fMask |= MIIM_ID;
        menuInfoDriveOffline.wID = idCmdFirst + IDM_DRIVEMENU_OFFLINE;
        menuInfoDriveOffline.fMask |= MIIM_STATE;
        menuInfoDriveOffline.fState = MFS_ENABLED;
		if (checkOfflineItem)
			menuInfoDriveOffline.fState |= MFS_CHECKED;
        // Insert it into the submenu
        if (!InsertMenuItem(hDriveSubMenu, 
            1, // At position one
            TRUE, //  indicates the existing item by using its zero-based position. (For example, the first item in the menu has a position of 0.) 
            &menuInfoDriveOffline
            ))
            return HRESULT_FROM_WIN32(GetLastError());

        // Insert the submenu below the "share" item
        MENUITEMINFO hDriveSubMenuInfo;
        hDriveSubMenuInfo.cbSize = sizeof (MENUITEMINFO);
        hDriveSubMenuInfo.fMask = MIIM_SUBMENU | MIIM_STATE | MIIM_STRING;
        hDriveSubMenuInfo.fState = MFS_ENABLED;
        // TODO: obtener el texto del cliente/gui
        hDriveSubMenuInfo.dwTypeData = &m_info.streamSubMenuTitle[0];
        hDriveSubMenuInfo.hSubMenu = hDriveSubMenu;

        // Insert the subitem into the 
        if (!InsertMenuItem(hMenu,
            indexMenu++,
            TRUE,
            &hDriveSubMenuInfo
            ))
            return HRESULT_FROM_WIN32(GetLastError());

    }

    indexMenu++;
    InsertSeperator(hMenu, indexMenu);
    // Return an HRESULT value with the severity set to SEVERITY_SUCCESS. 
    // Set the code value to the offset of the largest command identifier 
    // that was assigned, plus one (1).
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(indexSubMenu));
}

IFACEMETHODIMP OCContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
    std::wstring command;

    // For the Unicode case, if the high-order word is not zero, the 
    // command's verb string is in lpcmi->lpVerbW. 
    if (HIWORD(((CMINVOKECOMMANDINFOEX*)pici)->lpVerbW))
    {
        command = ((CMINVOKECOMMANDINFOEX *)pici)->lpVerbW;
    } else {
        // If the command cannot be identified through the verb string, then
        // check the identifier offset.
        if (LOWORD(pici->lpVerb) == IDM_DRIVEMENU_ONLINE)
        {
            OnDriveMenuOnline(pici->hwnd);
        }
        else if (LOWORD(pici->lpVerb) == IDM_DRIVEMENU_OFFLINE)
        {
            OnDriveMenuOffline(pici->hwnd);
        }
        else
        {
        auto offset = LOWORD(pici->lpVerb);
        if (offset >= m_info.menuItems.size())
            return E_FAIL;

        command = m_info.menuItems[offset].command;
        }
    }

    OCClientInterface::SendRequest(command.data(), m_selectedFiles);
    return S_OK;
}

IFACEMETHODIMP OCContextMenu::GetCommandString(UINT_PTR idCommand,
    UINT uFlags, UINT *pwReserved, LPSTR pszName, UINT cchMax)
{
    if (idCommand < m_info.menuItems.size() && uFlags == GCS_VERBW) {
        return StringCchCopyW(reinterpret_cast<PWSTR>(pszName), cchMax,
            m_info.menuItems[idCommand].command.data());
    }
    return E_INVALIDARG;
}

void OCContextMenu::OnDriveMenuOffline(HWND hWnd)
{
    OCClientInterface::SetDownloadMode(std::wstring(m_szSelectedFile), false);
}

void OCContextMenu::OnDriveMenuOnline(HWND hWnd)
{
    OCClientInterface::SetDownloadMode(std::wstring(m_szSelectedFile), true);
}

#pragma endregion
