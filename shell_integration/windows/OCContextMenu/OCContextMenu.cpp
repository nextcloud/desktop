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
#include "OCClientInterface.h"

#include <shobjidl.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <StringUtil.h>
#include <strsafe.h>

extern long g_cDllRef;

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
    static const QITAB qit[] = {
        QITABENT(OCContextMenu, IContextMenu),
        QITABENT(OCContextMenu, IShellExtInit),
        {nullptr},
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
    LPCITEMIDLIST, LPDATAOBJECT pDataObj, HKEY)
{
    m_selectedFiles.clear();

    if (!pDataObj) {
        return E_INVALIDARG;
    }

    FORMATETC fe = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stm;

    if (SUCCEEDED(pDataObj->GetData(&fe, &stm))) {
        // Get an HDROP handle.
        HDROP hDrop = static_cast<HDROP>(GlobalLock(stm.hGlobal));
        if (hDrop) {
            UINT nFiles = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < nFiles; ++i) {
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
        if (m_info.icon) {
            mii.fMask |= MIIM_BITMAP;
            mii.hbmpItem = *m_info.icon;
        }

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

    // Return an HRESULT value with the severity set to SEVERITY_SUCCESS. 
    // Set the code value to the offset of the largest command identifier 
    // that was assigned, plus one (1).
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(indexSubMenu));
}

IFACEMETHODIMP OCContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
    std::wstring command;

    CMINVOKECOMMANDINFOEX *piciEx = nullptr;
    if (pici->cbSize == sizeof(CMINVOKECOMMANDINFOEX))
        piciEx = (CMINVOKECOMMANDINFOEX*)pici;

    // For the Unicode case, if the high-order word is not zero, the 
    // command's verb string is in lpcmi->lpVerbW. 
    if (piciEx
        && (piciEx->fMask & CMIC_MASK_UNICODE)
        && HIWORD(((CMINVOKECOMMANDINFOEX*)pici)->lpVerbW)) {

        command = piciEx->lpVerbW;

        // Verify that we handle the verb
        bool handled = false;
        for (auto &item : m_info.menuItems) {
            if (item.command == command) {
                handled = true;
                break;
            }
        }
        if (!handled)
            return E_FAIL;
    } else if (IS_INTRESOURCE(pici->lpVerb)) {
        // If the command cannot be identified through the verb string, then
        // check the identifier offset.
        auto offset = LOWORD(pici->lpVerb);
        if (offset >= m_info.menuItems.size())
            return E_FAIL;

        command = m_info.menuItems[offset].command;
    } else {
        return E_FAIL;
    }

    std::ignore = OCClientInterface::SendRequest(command, m_selectedFiles);
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

#pragma endregion
