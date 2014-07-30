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

#define	OVERLAY_ID					1
#define OVERLAY_GUID				L"{0960F09E-F328-48A3-B746-276B1E3C3722}"
#define OVERLAY_NAME				L"OwnCloudStatusOverlay"

#define REGISTRY_OVERLAY_KEY		L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers"
#define REGISTRY_CLSID				L"CLSID"
#define REGISTRY_IN_PROCESS			L"InprocServer32"
#define REGISTRY_THREADING			L"ThreadingModel"
#define REGISTRY_APARTMENT			L"Apartment"
#define REGISTRY_VERSION			L"Version"
#define REGISTRY_VERSION_NUMBER		L"1.0"

//Registry values for running
#define REGISTRY_ENABLE_OVERLAY		L"EnableOverlay"

#define GET_FILE_OVERLAY_ID		L"getFileIconId"

#define PORT				33001