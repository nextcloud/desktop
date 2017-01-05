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

#include "FileUtil.h"
#include "RegistryUtil.h"
#include "UtilConstants.h"

using namespace std;

bool FileUtil::IsChildFile(const wchar_t* rootFolder, vector<wstring>* files)
{
    for(vector<wstring>::iterator it = files->begin(); it != files->end(); it++)
    {
        wstring file = *it;

        size_t found = file.find(rootFolder);

        if(found != string::npos)
        {
            return true;    
        }
    }

    return false;
}

bool FileUtil::IsChildFile(const wchar_t* rootFolder, const wchar_t* file)
{
    wstring* f = new wstring(file);

    size_t found = f->find(rootFolder);

    if(found != string::npos)
    {
        return true;    
    }
    
    return false;
}

bool FileUtil::IsChildFileOfRoot(std::vector<std::wstring>* files) 
{
    wstring* rootFolder = new wstring();
    bool needed = false;

    if(RegistryUtil::ReadRegistry(REGISTRY_ROOT_KEY, REGISTRY_FILTER_FOLDER, rootFolder))
    {
        if(IsChildFile(rootFolder->c_str(), files))
        {
            needed = true;
        }
    }

    delete rootFolder;
    return needed;
}

bool FileUtil::IsChildFileOfRoot(const wchar_t* filePath)
{
    wstring* rootFolder = new wstring();
    bool needed = false;
    
    if(RegistryUtil::ReadRegistry(REGISTRY_ROOT_KEY, REGISTRY_FILTER_FOLDER, rootFolder))
    {
        if(FileUtil::IsChildFile(rootFolder->c_str(), filePath))
        {
            needed = true;
        }
    }

    delete rootFolder;
    return needed;
}