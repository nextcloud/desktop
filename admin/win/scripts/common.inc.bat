@echo off
REM SPDX-FileCopyrightText: 2019 Michael Schuster
REM SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
REM SPDX-License-Identifier: GPL-2.0-or-later

Rem ********************************************************************************************
rem     "common environment Variables"
Rem ********************************************************************************************

set "BUILD_ARCH=%~2"
set "CMAKE_GENERATOR=Ninja"
set "BRANCH_NAME=%~3"

Rem CRAFT_PATH can be overridden externally; fall back to conventional KDE Craft locations.
if "%CRAFT_PATH%" == "" (
    if "%BUILD_ARCH%" == "Win32" ( set "CRAFT_PATH=c:\Craft32" )
    if "%BUILD_ARCH%" == "Win64" ( set "CRAFT_PATH=c:\Nextcloud\%BRANCH_NAME%\windows-msvc2022_64-cl" )
)

if "%BUILD_ARCH%" == "Win32" ( set "QT_PATH=%CRAFT_PATH%" )
if "%BUILD_ARCH%" == "Win32" ( set "PATH=%CRAFT_PATH%\bin;%CRAFT_PATH%\dev-utils\bin;%PATH%" )
if "%BUILD_ARCH%" == "Win32" ( set "QT_BIN_PATH=%CRAFT_PATH%\bin" )
if "%BUILD_ARCH%" == "Win32" ( set "QT_PREFIX=%CRAFT_PATH%" )

if "%BUILD_ARCH%" == "Win64" ( set "QT_PATH=%CRAFT_PATH%" )
if "%BUILD_ARCH%" == "Win64" ( set "PATH=%CRAFT_PATH%\bin;%CRAFT_PATH%\dev-utils\bin;%PATH%" )
if "%BUILD_ARCH%" == "Win64" ( set "QT_BIN_PATH=%CRAFT_PATH%\bin" )
if "%BUILD_ARCH%" == "Win64" ( set "QT_PREFIX=%CRAFT_PATH%" )

Rem Default EXTRA_DEPLOY_PATH when not already set by the calling script.
Rem This path is optional; if it does not exist or is empty the collect step silently skips it.
if "%EXTRA_DEPLOY_PATH%" == "" set "EXTRA_DEPLOY_PATH=%PROJECT_PATH%/deploy-extra/%BUILD_TYPE%/%BUILD_ARCH%"
