@echo off
REM SPDX-FileCopyrightText: 2019 Michael Schuster
REM SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
REM SPDX-License-Identifier: GPL-2.0-or-later
setlocal EnableDelayedExpansion

Rem ********************************************************************************************
rem     "desktop - Build for Windows 64-bit and/or 32-bit"
Rem ********************************************************************************************

call "%~dp0/defaults.inc.bat" %1

Rem ********************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

for %%G in (%BUILD_TARGETS%) do (
    if "%BUILD_TYPE%" == "Debug" (
        set "DLL_SUFFIX=d"
    ) else (
        set "DLL_SUFFIX="
    )

    echo "**** build desktop for %%G (%~nx0)."
    start "single-build-desktop.bat %BUILD_TYPE% %%G" /D "%PROJECT_PATH%/" /B /wait "%~dp0/single-build-desktop.bat" %BUILD_TYPE% %%G

    if !ERRORLEVEL! neq 0 exit !ERRORLEVEL!
)

Rem ********************************************************************************************

exit 0
