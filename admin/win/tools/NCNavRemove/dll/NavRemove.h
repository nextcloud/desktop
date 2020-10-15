/*
 * Copyright (C) by Michael Schuster <michael@schuster.ms>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#pragma once

#include <windows.h>

// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the _NAVREMOVE_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// NAVREMOVE_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.

#ifdef _NAVREMOVE_EXPORTS
#define NAVREMOVE_API __declspec(dllexport)
#else
#define NAVREMOVE_API __declspec(dllimport)
#endif

NAVREMOVE_API HRESULT RemoveNavigationPaneEntries();
