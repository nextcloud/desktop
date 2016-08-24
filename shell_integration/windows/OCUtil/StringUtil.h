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

#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#pragma once

#include <string>

class __declspec(dllexport) StringUtil {
public:
	static std::string  toUtf8(const wchar_t* utf16, int len = -1);
	static std::wstring toUtf16(const char* utf8, int len = -1);

	template<class T>
	static bool begins_with(const T& input, const T& match)
	{
		return input.size() >= match.size()
			&& std::equal(match.begin(), match.end(), input.begin());
	}
};

#endif // STRINGUTIL_H
