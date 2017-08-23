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

extern HINSTANCE g_hInst;
extern long g_cDllRef;

#define IDM_SHARE 0
#define IDM_COPYLINK 1
#define IDM_EMAILLINK 2

OCContextMenu::OCContextMenu(void) 
    : m_cRef(1)
{
    InterlockedIncrement(&g_cDllRef);
}

OCContextMenu::~OCContextMenu(void)
{
    InterlockedDecrement(&g_cDllRef);
}


void OCContextMenu::OnVerbShare(HWND hWnd)
{
    OCClientInterface::RequestShare(std::wstring(m_szSelectedFile));
}

void OCContextMenu::OnVerbCopyLink(HWND hWnd)
{
    OCClientInterface::RequestCopyLink(std::wstring(m_szSelectedFile));
}

void OCContextMenu::OnVerbEmailLink(HWND hWnd)
{
    OCClientInterface::RequestEmailLink(std::wstring(m_szSelectedFile));
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
    if (!pDataObj) {
        return E_INVALIDARG;
    }

    HRESULT hr = E_FAIL;

    FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stm;

    if (SUCCEEDED(pDataObj->GetData(&fe, &stm))) {
        // Get an HDROP handle.
        HDROP hDrop = static_cast<HDROP>(GlobalLock(stm.hGlobal));
        if (hDrop) {
            // Ignore multi-selections
            UINT nFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
            if (nFiles == 1) {
                // Get the path of the file.
                if (0 != DragQueryFile(hDrop, 0, m_szSelectedFile,  ARRAYSIZE(m_szSelectedFile)))
                {
                    hr = S_OK;
                }
            }

            GlobalUnlock(stm.hGlobal);
        }

        ReleaseStgMedium(&stm);
    }

    // If any value other than S_OK is returned from the method, the context 
    // menu item is not displayed.
    return hr;
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

    OCClientInterface::ContextMenuInfo info = OCClientInterface::FetchInfo();
    bool skip = true;
    size_t selectedFileLength = wcslen(m_szSelectedFile);
    for (const std::wstring path : info.watchedDirectories) {
        if (StringUtil::isDescendantOf(m_szSelectedFile, selectedFileLength, path)) {
            skip = false;
            break;
        }
    }

    if (skip) {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));
    }

    InsertSeperator(hMenu, indexMenu++);

    HMENU hSubmenu = CreateMenu();
    {
        MENUITEMINFO mii = { sizeof(mii) };
        mii.fMask = MIIM_SUBMENU | MIIM_FTYPE | MIIM_STRING;
        mii.hSubMenu = hSubmenu;
        mii.fType = MFT_STRING;
        mii.dwTypeData = &info.contextMenuTitle[0];

        if (!InsertMenuItem(hMenu, indexMenu++, TRUE, &mii))
            return HRESULT_FROM_WIN32(GetLastError());
    }
    InsertSeperator(hMenu, indexMenu++);

    UINT indexSubMenu = 0;
    {
        assert(!info.shareMenuTitle.empty());
        MENUITEMINFO mii = { sizeof(mii) };
        mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STRING;
        mii.wID = idCmdFirst + IDM_SHARE;
        mii.fType = MFT_STRING;
        mii.dwTypeData = &info.shareMenuTitle[0];

        if (!InsertMenuItem(hSubmenu, indexSubMenu++, TRUE, &mii))
            return HRESULT_FROM_WIN32(GetLastError());
    }
    {
        assert(!info.copyLinkMenuTitle.empty());
        MENUITEMINFO mii = { sizeof(mii) };
        mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STRING;
        mii.wID = idCmdFirst + IDM_COPYLINK;
        mii.fType = MFT_STRING;
        mii.dwTypeData = &info.copyLinkMenuTitle[0];

        if (!InsertMenuItem(hSubmenu, indexSubMenu++, TRUE, &mii))
            return HRESULT_FROM_WIN32(GetLastError());
    }
    {
        assert(!info.emailLinkMenuTitle.empty());
        MENUITEMINFO mii = { sizeof(mii) };
        mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STRING;
        mii.wID = idCmdFirst + IDM_EMAILLINK;
        mii.fType = MFT_STRING;
        mii.dwTypeData = &info.emailLinkMenuTitle[0];

        if (!InsertMenuItem(hSubmenu, indexSubMenu++, TRUE, &mii))
            return HRESULT_FROM_WIN32(GetLastError());
    }


    // Return an HRESULT value with the severity set to SEVERITY_SUCCESS. 
    // Set the code value to the offset of the largest command identifier 
    // that was assigned, plus one (1).
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(IDM_EMAILLINK + 1));
}

IFACEMETHODIMP OCContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{

    // For the Unicode case, if the high-order word is not zero, the 
    // command's verb string is in lpcmi->lpVerbW. 
    if (HIWORD(((CMINVOKECOMMANDINFOEX*)pici)->lpVerbW))
    {
        // Is the verb supported by this context menu extension?
        if (StrCmpIW(((CMINVOKECOMMANDINFOEX*)pici)->lpVerbW, L"ocshare") == 0) {
            OnVerbShare(pici->hwnd);
        }
        else if (StrCmpIW(((CMINVOKECOMMANDINFOEX*)pici)->lpVerbW, L"occopylink") == 0) {
            OnVerbCopyLink(pici->hwnd);
        }
        else if (StrCmpIW(((CMINVOKECOMMANDINFOEX*)pici)->lpVerbW, L"ocemaillink") == 0) {
            OnVerbEmailLink(pici->hwnd);
        }
        else {
            // If the verb is not recognized by the context menu handler, it 
            // must return E_FAIL to allow it to be passed on to the other 
            // context menu handlers that might implement that verb.
            return E_FAIL;
        }
    }

    // If the command cannot be identified through the verb string, then 
    // check the identifier offset.
    else
    {
        // Is the command identifier offset supported by this context menu 
        // extension?
        if (LOWORD(pici->lpVerb) == IDM_SHARE) {
            OnVerbShare(pici->hwnd);
        }
        else if (LOWORD(pici->lpVerb) == IDM_COPYLINK) {
            OnVerbCopyLink(pici->hwnd);
        }
        else if (LOWORD(pici->lpVerb) == IDM_EMAILLINK) {
            OnVerbEmailLink(pici->hwnd);
        }
        else {
            // If the verb is not recognized by the context menu handler, it 
            // must return E_FAIL to allow it to be passed on to the other 
            // context menu handlers that might implement that verb.
            return E_FAIL;
        }
    }

    return S_OK;
}

IFACEMETHODIMP OCContextMenu::GetCommandString(UINT_PTR idCommand,
    UINT uFlags, UINT *pwReserved, LPSTR pszName, UINT cchMax)
{
    HRESULT hr = E_INVALIDARG;

    switch (idCommand) {
    case IDM_SHARE:
        if (uFlags == GCS_VERBW) {
            // GCS_VERBW is an optional feature that enables a caller to
            // discover the canonical name for the verb passed in through
            // idCommand.
            hr = StringCchCopy(reinterpret_cast<PWSTR>(pszName), cchMax,
                L"OCShareViaOC");
        }
        break;
    case IDM_COPYLINK:
        if (uFlags == GCS_VERBW) {
            hr = StringCchCopy(reinterpret_cast<PWSTR>(pszName), cchMax,
                L"OCCopyLink");
        }
        break;
    case IDM_EMAILLINK:
        if (uFlags == GCS_VERBW) {
            hr = StringCchCopy(reinterpret_cast<PWSTR>(pszName), cchMax,
                L"OCEmailLink");
        }
        break;
    default:
        break;
    }

    // If the idCommand or uFlags is not supported by this context menu
    // extension handler, return E_INVALIDARG.

    return hr;
}

#pragma endregion