@echo off
REM SPDX-FileCopyrightText: 2019 Michael Schuster
REM SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
REM SPDX-License-Identifier: GPL-2.0-or-later
setlocal EnableDelayedExpansion
cls

echo "***** Build started. (%~nx0)"

Rem ********************************************************************************************
rem     "Build everything"
Rem ********************************************************************************************

call "%~dp0/defaults.inc.bat" %1

Rem ********************************************************************************************

echo "* BUILD_TYPE=%BUILD_TYPE%"
echo "* BUILD_TARGETS=%BUILD_TARGETS%"

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html


Rem ********************************************************************************************
rem     "check for required environment variables"
Rem ********************************************************************************************

call :testEnv PROJECT_PATH
call :testEnv BUILD_TYPE
call :testEnv BUILD_TARGETS
call :testEnv Png2Ico_EXECUTABLE
call :testEnv VS_VERSION
call :testEnv VCINSTALLDIR
call :testEnv WIN_GIT_PATH

if %ERRORLEVEL% neq 0 goto onError

Rem ********************************************************************************************
rem     "desktop"
Rem ********************************************************************************************

echo "***** build desktop."
start "build-desktop.bat %BUILD_TYPE%" /D "%PROJECT_PATH%/" /B /wait "%~dp0/build-desktop.bat" %BUILD_TYPE%
if %ERRORLEVEL% neq 0 goto onError


Rem ********************************************************************************************
rem     "collect files for the installer"
Rem ********************************************************************************************

echo "***** collect files for the installer."
start "build-installer-collect.bat %BUILD_TYPE%" /D "%PROJECT_PATH%/" /B /wait "%~dp0/build-installer-collect.bat" %BUILD_TYPE%
if %ERRORLEVEL% neq 0 goto onError


Rem ********************************************************************************************
rem     "build the MSI installer"
Rem ********************************************************************************************

if "%BUILD_INSTALLER_MSI%" == "0" (
    echo "** Don't build the MSI installer (disabled by BUILD_INSTALLER_MSI)"
) else (
    echo "***** build the MSI installer."
    start "build-installer-msi.bat %BUILD_TYPE%" /D "%PROJECT_PATH%/" /B /wait "%~dp0/build-installer-msi.bat" %BUILD_TYPE%
)
if %ERRORLEVEL% neq 0 goto onError


Rem ********************************************************************************************

echo "***** Build finished. (%~nx0)"
exit 0

:onError
echo "***** Build FAILED! (%~nx0)"
if %ERRORLEVEL% neq 0 exit %ERRORLEVEL%
if !ERRORLEVEL! neq 0 exit !ERRORLEVEL!
exit 1

:testEnv
if "!%*!" == "" (
    echo "Missing environment variable: %*"
    exit /B 1
)
exit /B
