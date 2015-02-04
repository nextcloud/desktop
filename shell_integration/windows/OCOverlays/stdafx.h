/**
 *  Copyright (c) 2000-2013 Liferay, Inc. All rights reserved.
 *  
 *  This library is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU Lesser General Public License as published by the Free
 *  Software Foundation; either version 2.1 of the License, or (at your option)
 *  any later version.
 *  
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 *  details.
 */

#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501

#include "CommunicationSocket.h"
#include "RegistryUtil.h"
#include "OverlayConstants.h"
#include "FileUtil.h"

#include <string>
#include <new>
#include <Guiddef.h>
#include <windows.h>
#include <Shlwapi.h>
#include <shlobj.h>
#include <unknwn.h> 
#include <vector>
#include <strsafe.h>
