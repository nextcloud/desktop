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

#include "stdafx.h"

#include "RegistryUtil.h"

using namespace std;

#define SIZE 4096

bool RegistryUtil::ReadRegistry(const wchar_t* key, const wchar_t* name, int* result)
{
    wstring* strResult = new wstring();

    if(!ReadRegistry(key, name, strResult))
    {
        return false;
    }

    *result = stoi( strResult->c_str() );

    return true;
}

bool RegistryUtil::ReadRegistry(const wchar_t* key, const wchar_t* name, wstring* result)
{
    HRESULT hResult;

    HKEY rootKey = NULL;

    hResult = HRESULT_FROM_WIN32(RegOpenKeyEx(HKEY_CURRENT_USER, (LPCWSTR)key, NULL, KEY_READ, &rootKey));

    if(!SUCCEEDED(hResult))
    {
        return false;
    }

    wchar_t value[SIZE];
    DWORD value_length = SIZE;
    
    hResult = RegQueryValueEx(rootKey, (LPCWSTR)name, NULL, NULL, (LPBYTE)value, &value_length );

    if(!SUCCEEDED(hResult))
    {
        return false;
    }

    result->append(value);

    HRESULT hResult2 = RegCloseKey(rootKey);

    if (!SUCCEEDED(hResult2))
    {
        return false;
    }

    return true;
}
