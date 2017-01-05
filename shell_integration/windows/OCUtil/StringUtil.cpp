/**
* Copyright (c) 2014 ownCloud GmbH. All rights reserved.
*
* This library is free software; you can redistribute it and/or modify it under
* the terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 2.1 of the License
*
* This library is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
* details.
*/

#include "stdafx.h"

#include <locale>
#include <string>
#include <codecvt>

#include "StringUtil.h"

std::string StringUtil::toUtf8(const wchar_t *utf16, int len)
{
    if (len < 0) {
        len = (int) wcslen(utf16);
    }
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t> > converter;
    return converter.to_bytes(utf16, utf16+len);
}

std::wstring StringUtil::toUtf16(const char *utf8, int len)
{
    if (len < 0) {
        len = (int) strlen(utf8);
    }
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t> > converter;
    return converter.from_bytes(utf8, utf8+len);
}
