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
 * 
 * Parts of this file are based on:
 * https://www.codeproject.com/articles/570751/devmsi-an-example-cplusplus-msi-wix-deferred-custo
 * 
 * Licensed under the The Code Project Open License (CPOL):
 * https://www.codeproject.com/info/cpol10.aspx
 * 
 */

#include "NCMsiHelper.h"
#include <MsiQuery.h>
#include <vector>

/**
 *  Sets up logging for MSIs and then calls the appropriate custom action with argc/argv parameters.
 * 
 *  MSI deferred custom action dlls have to handle parameters (properties) a little differently,
 *  since the deferred action may not have an active session when it begins.  Since the easiest
 *  way to pass parameter(s) is to put them all into a CustomActionData property and then retrieve it,
 *  the easiest thing to do on this ( C/C++ ) end is to pull the parameter and then split it into
 *  a list of parameter(s) that we need.
 * 
 *  For this implementation, it made sense to treat the single string provided in CustomActionData
 *  as if it were a command line, and then parse it out just as if it were a command line.  Obviously,
 *  the "program name" isn't going to be the first argument unless the MSI writer is pedantic, but
 *  otherwise it seems to be a good way to do it.
 * 
 *  Since all entry points need to do this same work, it was easiest to have a single function that
 *  would do the setup, pull the CustomActionData parameter, split it into an argc/argv style of
 *  argument list, and then pass that argument list into a function that actually does something
 *  interesting.
 *
 *  @param hInstall The hInstall parameter provided by MSI/WiX.
 *  @param func The function to be called with argc/argv parameters.
 *  @param actionName The text description of the function.  It will be put in the log.
 *  @return Returns ERROR_SUCCESS or ERROR_INSTALL_FAILURE.
 */
UINT CustomActionArgcArgv(MSIHANDLE hInstall, CUSTOM_ACTION_ARGC_ARGV func, LPCSTR actionName)
{
    LPWSTR pszCustomActionData = nullptr, *argv = nullptr;

    HRESULT hr = WcaInitialize(hInstall, actionName);
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");
    
    // Retrieve our custom action property. This is one of
    // only three properties we can request on a Deferred
    // Custom Action.  So, we assume the caller puts all
    // parameters in this one property.
    hr = WcaGetProperty(L"CustomActionData", &pszCustomActionData);
    ExitOnFailure(hr, "Failed to get Custom Action Data.");
    WcaLog(LOGMSG_STANDARD, "Custom Action Data = '%ls'.", pszCustomActionData);

    // Convert the string retrieved into a standard argc/arg layout
    // (ignoring the fact that the first parameter is whatever was
    // passed, not necessarily the application name/path).
    int argc = 0;
    argv = CommandLineToArgvW(pszCustomActionData, &argc);
    if (argv) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        ExitOnFailure(hr, "Failed to convert Custom Action Data to argc/argv.");
    }

    hr = (func)(argc, argv);
    ExitOnFailure(hr, "Custom action failed");

LExit:
    // Resource freeing here!
    ReleaseStr(pszCustomActionData);
    if (argv)
        LocalFree(argv);

    return WcaFinalize(SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE);
}

UINT __stdcall ExecNsisUninstaller(MSIHANDLE hInstall)
{
    return CustomActionArgcArgv(hInstall, DoExecNsisUninstaller, "ExecNsisUninstaller");
}

UINT __stdcall RemoveNavigationPaneEntries(MSIHANDLE hInstall)
{
    return CustomActionArgcArgv(hInstall, DoRemoveNavigationPaneEntries, "RemoveNavigationPaneEntries");
}

UINT LogMsiInfoMessage(MSIHANDLE hInstall, const TCHAR *format, ...)
{
    TCHAR szFormatted[MAX_PATH];

    va_list args;
    va_start(args, format);
    vswprintf(szFormatted, MAX_PATH, format, args);
    va_end(args);

    PMSIHANDLE hRecord = ::MsiCreateRecord(1);
    ::MsiRecordSetString(hRecord, 0, szFormatted);

    // we are always logging a message as info, as error will bring a popup that we don't want, just logs
    return MsiProcessMessage(hInstall, INSTALLMESSAGE_INFO, hRecord);
}

UINT __stdcall CloseWindowByClassName(MSIHANDLE hInstall)
{
    const auto windowClassPropertyName = _T("WNDCLASSNAMETOCLOSE");
    DWORD windowClassNameSize = 0;
    if (MsiGetProperty(hInstall, windowClassPropertyName, _T(""), &windowClassNameSize) != ERROR_MORE_DATA) {
        LogMsiInfoMessage(hInstall,
                          _T("ERROR: Custom action CloseWindowByClassName. MsiGetProperty failed for windowClassPropertyName: %s"),
                          windowClassPropertyName);
        return ERROR_BAD_ARGUMENTS;
    }

    if (windowClassNameSize <= 0) {
        LogMsiInfoMessage(hInstall, _T("ERROR: Custom action CloseWindowByClassName. classNameSize is <= 0!"));
        return ERROR_BAD_ARGUMENTS;
    }

    ++windowClassNameSize;

    std::vector<TCHAR> windowClassNameValue(windowClassNameSize, 0);
    std::vector<char> vec;
    const auto getPropertyRes = MsiGetProperty(hInstall, windowClassPropertyName, windowClassNameValue.data(), &windowClassNameSize);
    if (getPropertyRes != ERROR_SUCCESS) {
        LogMsiInfoMessage(hInstall, _T("ERROR: Custom action CloseWindowByClassName. MsiGetProperty failed for windowClassPropertyName: %s with code: %d"),
            windowClassNameValue.data(),
            getPropertyRes);
        return getPropertyRes;
    }

    if (windowClassNameSize <= 0) {
        LogMsiInfoMessage(hInstall, _T("ERROR: Custom action CloseWindowByClassName. Final classNameSize is <= 0!"));
        return ERROR_BAD_ARGUMENTS;
    }

    LogMsiInfoMessage(hInstall, _T("Custom action CloseWindowByClassName is running for windowClassNameValue: %s"), windowClassNameValue.data());

    const auto windowToCloseHandle = FindWindow(windowClassNameValue.data(), NULL);
    if (windowToCloseHandle == NULL) {
        LogMsiInfoMessage(hInstall, _T("WARNING: Custom action CloseWindowByClassName. windowToCloseHandle is NULL."));
        // FindWindow will return NULL if the window is not currently running, so not an error
        return ERROR_SUCCESS;
    }

    LogMsiInfoMessage(hInstall, _T("Custom action CloseWindowByClassName. Sending WM_CLOSE message to windowClassNameValue: %s"), windowClassNameValue.data());

    SendMessage(windowToCloseHandle, WM_CLOSE, 0, 0);

    return ERROR_SUCCESS;
}

/**
 * DllMain - Initialize and cleanup WiX custom action utils.
 */
extern "C" BOOL WINAPI DllMain(
    __in HINSTANCE hInst,
    __in ULONG ulReason,
    __in LPVOID
    )
{
    switch(ulReason)
    {
    case DLL_PROCESS_ATTACH:
        WcaGlobalInitialize(hInst);
        break;

    case DLL_PROCESS_DETACH:
        WcaGlobalFinalize();
        break;
    }

    return TRUE;
}
