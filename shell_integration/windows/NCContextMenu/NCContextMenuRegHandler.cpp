/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "NCContextMenuRegHandler.h"
#include "RegDelnode.h"
#include <strsafe.h>
#include <objbase.h>

namespace {

HRESULT SetHKCRRegistryKeyAndValue(PCWSTR pszSubKey, PCWSTR pszValueName, PCWSTR pszData)
{
    HRESULT hr = 0;
    HKEY hKey = nullptr;

    // Creates the specified registry key. If the key already exists, the 
    // function opens it. 
    hr = HRESULT_FROM_WIN32(RegCreateKeyEx(HKEY_CLASSES_ROOT, pszSubKey, 0,
        nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr));

    if (SUCCEEDED(hr))
    {
        DWORD cbData = 0;
        const BYTE * lpData = nullptr;

        if (pszData)
        {
            // Set the specified value of the key.
            cbData = lstrlen(pszData) * sizeof(*pszData);
            lpData = reinterpret_cast<const BYTE *>(pszData);
        }
        else
        {
            cbData = 0;
            lpData = nullptr;
        }

        hr = HRESULT_FROM_WIN32(RegSetValueEx(hKey, pszValueName, 0,
            REG_SZ, lpData, cbData));

        RegCloseKey(hKey);
    }

    return hr;
}

HRESULT GetHKCRRegistryKeyAndValue(PCWSTR pszSubKey, PCWSTR pszValueName, PWSTR pszData, DWORD cbData)
{
    HRESULT hr = 0;
    HKEY hKey = nullptr;

    // Try to open the specified registry key. 
    hr = HRESULT_FROM_WIN32(RegOpenKeyEx(HKEY_CLASSES_ROOT, pszSubKey, 0,
        KEY_READ, &hKey));

    if (SUCCEEDED(hr))
    {
        // Get the data for the specified value name.
        hr = HRESULT_FROM_WIN32(RegQueryValueEx(hKey, pszValueName, nullptr,
            nullptr, reinterpret_cast<LPBYTE>(pszData), &cbData));

        RegCloseKey(hKey);
    }

    return hr;
}

}

HRESULT NCContextMenuRegHandler::RegisterInprocServer(PCWSTR pszModule, const CLSID& clsid, PCWSTR pszFriendlyName, PCWSTR pszThreadModel)
{
    if (!pszModule || !pszThreadModel)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = 0;

    wchar_t szCLSID[MAX_PATH];
    StringFromGUID2(clsid, szCLSID, ARRAYSIZE(szCLSID));

    wchar_t szSubkey[MAX_PATH];

    // Create the HKCR\CLSID\{<CLSID>} key.
    hr = StringCchPrintf(szSubkey, ARRAYSIZE(szSubkey), LR"(CLSID\%s)", szCLSID);
    if (SUCCEEDED(hr))
    {
        hr = SetHKCRRegistryKeyAndValue(szSubkey, nullptr, pszFriendlyName);

        // Create the HKCR\CLSID\{<CLSID>}\ContextMenuOptIn subkey.
        if (SUCCEEDED(hr)) {
            hr = SetHKCRRegistryKeyAndValue(szSubkey, L"ContextMenuOptIn", nullptr);
        }

        // Create the HKCR\CLSID\{<CLSID>}\InprocServer32 key.
        if (SUCCEEDED(hr))
        {
            hr = StringCchPrintf(szSubkey, ARRAYSIZE(szSubkey),
                LR"(CLSID\%s\InprocServer32)", szCLSID);
            if (SUCCEEDED(hr))
            {
                // Set the default value of the InprocServer32 key to the 
                // path of the COM module.
                hr = SetHKCRRegistryKeyAndValue(szSubkey, nullptr, pszModule);
                if (SUCCEEDED(hr))
                {
                    // Set the threading model of the component.
                    hr = SetHKCRRegistryKeyAndValue(szSubkey,
                        L"ThreadingModel", pszThreadModel);
                }
            }
        }
    }

    return hr;
}

HRESULT NCContextMenuRegHandler::UnregisterInprocServer(const CLSID& clsid)
{
    HRESULT hr = S_OK;

    wchar_t szCLSID[MAX_PATH];
    StringFromGUID2(clsid, szCLSID, ARRAYSIZE(szCLSID));

    wchar_t szSubkey[MAX_PATH];

    // Delete the HKCR\CLSID\{<CLSID>} key.
    hr = StringCchPrintf(szSubkey, ARRAYSIZE(szSubkey), LR"(CLSID\%s)", szCLSID);
    if (SUCCEEDED(hr))
    {
        hr = HRESULT_FROM_WIN32(RegDelnode(HKEY_CLASSES_ROOT, szSubkey));
    }

    return hr;
}


HRESULT NCContextMenuRegHandler::RegisterShellExtContextMenuHandler(
    PCWSTR pszFileType, const CLSID& clsid, PCWSTR pszFriendlyName)
{
    if (!pszFileType)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = 0;

    wchar_t szCLSID[MAX_PATH];
    StringFromGUID2(clsid, szCLSID, ARRAYSIZE(szCLSID));

    wchar_t szSubkey[MAX_PATH];

    // If pszFileType starts with '.', try to read the default value of the 
    // HKCR\<File Type> key which contains the ProgID to which the file type 
    // is linked.
    if (*pszFileType == L'.')
    {
        wchar_t szDefaultVal[260];
        hr = GetHKCRRegistryKeyAndValue(pszFileType, nullptr, szDefaultVal,
            sizeof(szDefaultVal));

        // If the key exists and its default value is not empty, use the 
        // ProgID as the file type.
        if (SUCCEEDED(hr) && szDefaultVal[0] != L'\0')
        {
            pszFileType = szDefaultVal;
        }
    }

    // Create the key HKCR\<File Type>\shellex\ContextMenuHandlers\{friendlyName>}
    hr = StringCchPrintf(szSubkey, ARRAYSIZE(szSubkey),
        LR"(%s\shellex\ContextMenuHandlers\%s)", pszFileType, pszFriendlyName);
    if (SUCCEEDED(hr))
    {
        // Set the default value of the key.
        hr = SetHKCRRegistryKeyAndValue(szSubkey, nullptr, szCLSID);
    }

    return hr;
}

HRESULT NCContextMenuRegHandler::UnregisterShellExtContextMenuHandler(
    PCWSTR pszFileType, PCWSTR pszFriendlyName)
{
    if (!pszFileType)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = 0;
    
    wchar_t szSubkey[MAX_PATH];

    // If pszFileType starts with '.', try to read the default value of the 
    // HKCR\<File Type> key which contains the ProgID to which the file type 
    // is linked.
    if (*pszFileType == L'.')
    {
        wchar_t szDefaultVal[260];
        hr = GetHKCRRegistryKeyAndValue(pszFileType, nullptr, szDefaultVal,
            sizeof(szDefaultVal));

        // If the key exists and its default value is not empty, use the 
        // ProgID as the file type.
        if (SUCCEEDED(hr) && szDefaultVal[0] != L'\0')
        {
            pszFileType = szDefaultVal;
        }
    }

    // Remove the HKCR\<File Type>\shellex\ContextMenuHandlers\{friendlyName} key.
    hr = StringCchPrintf(szSubkey, ARRAYSIZE(szSubkey),
        LR"(%s\shellex\ContextMenuHandlers\%s)", pszFileType, pszFriendlyName);
    if (SUCCEEDED(hr))
    {
        hr = HRESULT_FROM_WIN32(RegDelnode(HKEY_CLASSES_ROOT, szSubkey));
    }

    return hr;
}
