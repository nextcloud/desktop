/*
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

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
