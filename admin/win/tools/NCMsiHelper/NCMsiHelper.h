/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later OR CPOL-1.02
 * 
 * Parts of this file are based on:
 * https://www.codeproject.com/articles/570751/devmsi-an-example-cplusplus-msi-wix-deferred-custo
 * 
 * Licensed under the The Code Project Open License (CPOL):
 * https://www.codeproject.com/info/cpol10.aspx
 */

/**
 * Function prototypes for external "C" interfaces into the DLL.
 *
 * This project builds a "hybrid" DLL that will work either from
 * a MSI Custom Action environment or from an external C program.
 * The former routes through "C" interface functions defined in 
 * CustomAction.def.  The latter uses the interfaces defined here.
 *
 * This header is suitable for inclusion by a project wanting to
 * call these methods.  Note that _NCMSIHELPER_EXPORTS should not be
 * defined for the accessing application source code.
 */
#pragma once

#include <windows.h>

#ifdef _NCMSIHELPER_EXPORTS
#  pragma comment (lib, "newdev")
#  pragma comment (lib, "setupapi")
#  pragma comment (lib, "msi")
#  pragma comment (lib, "dutil")
#  pragma comment (lib, "wcautil")
#  pragma comment (lib, "Version")

#  include <cstdlib>
#  include <string>
#  include <tchar.h>
#  include <msiquery.h>
#  include <lmerr.h>

// WiX Header Files:
#  include <wcautil.h>
#  include <strutil.h>

#  define NCMSIHELPER_API __declspec(dllexport)
#else
#  define NCMSIHELPER_API __declspec(dllimport)
#endif

/**
 * Runs the NSIS uninstaller and waits for its completion.
 *
 * argc MUST be 2.
 *
 * argv[0] is APPLICATION_EXECUTABLE, e.g. "nextcloud"
 * argv[1] is the full path to "Uninstall.exe"
 *
 * @param argc  The count of valid arguments in argv.
 * @param argv  An array of string arguments for the function.
 * @return Returns an HRESULT indicating success or failure.
 */
HRESULT NCMSIHELPER_API DoExecNsisUninstaller(int argc, LPWSTR *argv);


/**
 * Removes the Explorer's Navigation Pane entries.
 *
 * argc MUST be 1.
 *
 * argv[0] is APPLICATION_NAME, e.g. "Nextcloud"
 *
 * @param argc  The count of valid arguments in argv.
 * @param argv  An array of string arguments for the function.
 * @return Returns an HRESULT indicating success or failure.
 */
HRESULT NCMSIHELPER_API DoRemoveNavigationPaneEntries(int argc, LPWSTR *argv);

/**
 *  Standardized function prototype for NCMsiHelper.
 *
 *  Functions in NCMsiHelper can be called through the MSI Custom
 *  Action DLL or through an external C program.  Both
 *  methods expect to wrap things into this function prototype.
 *
 *  As a result, all functions defined in this header should
 *  conform to this function prototype.
 */
using CUSTOM_ACTION_ARGC_ARGV = NCMSIHELPER_API HRESULT(*)(int argc, LPWSTR *argv);
