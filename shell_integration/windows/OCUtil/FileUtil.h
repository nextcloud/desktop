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

#ifndef FILEUTIL_H
#define FILEUTIL_H

#pragma once

#pragma warning (disable : 4251)

#include <string>
#include <vector>

class __declspec(dllexport) FileUtil
{
public:
	FileUtil();

	~FileUtil();

	static bool IsChildFile(const wchar_t*, std::vector<std::wstring>*);
	static bool IsChildFile(const wchar_t*, const wchar_t*);
	static bool IsChildFileOfRoot(std::vector<std::wstring>*);
	static bool IsChildFileOfRoot(const wchar_t*);

private:	
};

#endif