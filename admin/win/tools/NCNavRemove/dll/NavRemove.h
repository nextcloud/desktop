/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
