// NCTools.h : include file for standard system include files
//

#pragma once

#include <WinSDKVer.h>

// // Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

// Windows Header Files
#include <windows.h>
#include <shellapi.h>
#include <Shlobj.h>
#include <psapi.h>
#include <wincred.h>

// C RunTime Header Files
#include <cstdlib>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include <variant>
