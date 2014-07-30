/**
* Copyright (c) 2014 ownCloud, Inc. All rights reserved.
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

#include <Windows.h>

#include "StringUtil.h"

char* StringUtil::toUtf8(const wchar_t *utf16, int len)
{
	int newlen = WideCharToMultiByte(CP_UTF8, 0, utf16, len, NULL, 0, NULL, NULL);
	char* str = new char[newlen];
	WideCharToMultiByte(CP_UTF8, 0, utf16, -1, str, newlen, NULL, NULL);
	return str;
}

wchar_t* StringUtil::toUtf16(const char *utf8, int len)
{
	int newlen = MultiByteToWideChar(CP_UTF8, 0, utf8, len, NULL, 0);
	wchar_t* wstr = new wchar_t[newlen];
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, newlen);
	return wstr;
}
