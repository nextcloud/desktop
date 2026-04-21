@echo off
REM SPDX-FileCopyrightText: 2019 Michael Schuster
REM SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
REM SPDX-License-Identifier: GPL-2.0-or-later

Rem ********************************************************************************************
rem     "Build defaults - environment Variables"
Rem ********************************************************************************************

Rem ********************************************************************************************
rem     "CUSTOMIZE HERE:"
Rem ********************************************************************************************

Rem These are the default build environment variables for all build scripts.
Rem You may specify them from outside.

Rem IMPORTANT: Keep an eye on all the slashes and backslashes in the paths.
Rem            If you intend to modify or specify them outside, keep the scheme!

Rem ********************************************************************************************
Rem Branding options

if "%APP_NAME%" == ""                       set "APP_NAME=Nextcloud"
if "%APP_NAME_SANITIZED%" == ""             set "APP_NAME_SANITIZED=Nextcloud"
if "%APPLICATION_NAME%" == ""               set "APPLICATION_NAME=Nextcloud Files Client"

if "%USE_BRANDING%" == ""                   set "USE_BRANDING=0"
if "%BUILD_TYPE%" == ""                     set "BUILD_TYPE=RelWithDebInfo"

Rem ********************************************************************************************
Rem Derive the Nextcloud desktop repository root from this script's location.
Rem The scripts live at admin/win/scripts/ inside the repo, so three levels up is the repo root.

if "%DESKTOP_REPO_PATH%" == "" (
    pushd "%~dp0\..\..\..\"
    set "DESKTOP_REPO_PATH=%CD%"
    popd
)

Rem ********************************************************************************************
Rem Build environment

Rem Comma separated list of build targets (default: Win64)
if "%BUILD_TARGETS%" == ""                  set "BUILD_TARGETS=Win64"

Rem PROJECT_PATH is the workspace root for build artifacts (install/, collect/, daily/).
Rem It should NOT be inside the repository.  The desktop source is at DESKTOP_REPO_PATH.
if "%PROJECT_PATH%" == ""                   set "PROJECT_PATH=c:/Nextcloud"

if "%Png2Ico_EXECUTABLE%" == ""             set "Png2Ico_EXECUTABLE=c:/Nextcloud/tools/png2ico.exe"

if "%VS_VERSION%" == ""                     set "VS_VERSION=2022"

Rem Required for Qt's windeployqt to find the VC Redist Setup (and for auto-discovery of signtool.exe)
if "%VCINSTALLDIR%" == "" (
    if "%VS_VERSION%" == "2017" (
        set "VCINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC"
    )
    if "%VS_VERSION%" == "2019" (
        set "VCINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC"
    )
    if "%VS_VERSION%" == "2022" (
        set "VCINSTALLDIR=C:\Program Files\Microsoft Visual Studio\2022\Community\VC"
    )
)

Rem Required for Git Bash's mkdir.exe (mkdir -p ...)
if "%WIN_GIT_PATH%" == ""                   set "WIN_GIT_PATH=C:\Program Files\Git"

Rem ********************************************************************************************
Rem Test run? 1 = just show environment variables, 0 = normal build (default)
if "%TEST_RUN%" == ""                       set "TEST_RUN=0"

Rem ********************************************************************************************
Rem Date and version for Desktop and Installer build

Rem Try to use our locale-independent helper
call "%~dp0/datetime.inc.bat"
if "%BUILD_DATE%" == "" (
    if "%_date%" == "" (
        set "BUILD_DATE=%date:~10,4%%date:~4,2%%date:~7,2%"
    ) else (
        set "BUILD_DATE=%_date:~0,4%%_date:~5,2%%_date:~8,2%"
    )
)

if "%VERSION_SUFFIX%" == ""                 set "VERSION_SUFFIX="

Rem Git tag for checkout when CHECKOUT_DESKTOP=1 (default: master for daily build)
if "%TAG_DESKTOP%" == ""                    set "TAG_DESKTOP=master"
if "%CRAFT_TAG_DESKTOP%" == ""              set "CRAFT_TAG_DESKTOP=master"

Rem ********************************************************************************************

Rem Git pull/checkout defaults.
Rem Since the build scripts now live inside the desktop repository, both options default to 0.
Rem Set PULL_DESKTOP=1 and CHECKOUT_DESKTOP=1 only when you want a fresh clone of the desktop
Rem source to a different location (e.g. for an isolated release build).
if "%PULL_DESKTOP%" == ""                   set "PULL_DESKTOP=0"
if "%CHECKOUT_DESKTOP%" == ""               set "CHECKOUT_DESKTOP=0"

Rem Branding overrides
if "%USE_BRANDING%" == "1" (
    set "PULL_DESKTOP=0"
    set "CHECKOUT_DESKTOP=0"
)

Rem Updater: ON = build, OFF = don't build (default)
if "%BUILD_UPDATER%" == ""                  set "BUILD_UPDATER=OFF"

Rem ********************************************************************************************
Rem Installer Options
Rem NSIS EXE installer: not supported; use MSI instead (BUILD_INSTALLER_MSI=1)
if "%BUILD_INSTALLER%" == ""                set "BUILD_INSTALLER=0"

Rem MSI installer: 1 = build (default), 0 = don't build
if "%BUILD_INSTALLER_MSI%" == ""            set "BUILD_INSTALLER_MSI=1"

if "%INSTALLER_OUTPUT_PATH%" == ""          set "INSTALLER_OUTPUT_PATH=%PROJECT_PATH%/daily/"

Rem ********************************************************************************************
Rem Code Signing Options: 1 = enable (default), 0 = disable
if "%USE_CODE_SIGNING%" == ""               set "USE_CODE_SIGNING=1"

Rem Vendor Name: Used for signing, also used by the installer
if "%APPLICATION_VENDOR%" == ""             set "APPLICATION_VENDOR=Nextcloud GmbH"

Rem PFX Key and Password - it may be a good idea to set the password outside (environment variables)
if "%CERTIFICATE_FILENAME%" == ""           set "CERTIFICATE_FILENAME="
if "%CERTIFICATE_CSP%" == ""                set "CERTIFICATE_CSP="
if "%CERTIFICATE_KEY_CONTAINER_NAME%" == "" set "CERTIFICATE_KEY_CONTAINER_NAME="
if "%CERTIFICATE_PASSWORD%" == ""           set "CERTIFICATE_PASSWORD="

if "%SIGN_FILE_DIGEST_ALG%" == ""           set "SIGN_FILE_DIGEST_ALG=sha256"
if "%SIGN_TIMESTAMP_URL%" == ""             set "SIGN_TIMESTAMP_URL=http://timestamp.digicert.com"
if "%SIGN_TIMESTAMP_DIGEST_ALG%" == ""      set "SIGN_TIMESTAMP_DIGEST_ALG=sha256"

Rem ********************************************************************************************
Rem Upload build: 1 = enable (default), 0 = disable
if "%UPLOAD_BUILD%" == ""                   set "UPLOAD_BUILD=1"

Rem Delete build after successful upload: 1 = delete, 0 = keep (default)
if "%UPLOAD_DELETE%" == ""                  set "UPLOAD_DELETE=0"

Rem Note: Storing SFTP_PATH outside in Windows's env leads to trouble due to the preceding slash!
if "%SFTP_PATH%" == ""                      set "SFTP_PATH=/var/www/html/desktop/daily/Windows"
if "%SFTP_SERVER%" == ""                    set "SFTP_SERVER="
if "%SFTP_USER%" == ""                      set "SFTP_USER="

Rem ********************************************************************************************
Rem CMake extra build flags (optional)

Rem Here you may define special flags for the Desktop build, e.g. the Update Server URL
Rem ( -DAPPLICATION_UPDATE_URL="https://your.url" ). see: desktop/build/config.h and version.h
if "%CMAKE_EXTRA_FLAGS_DESKTOP%" == ""      set "CMAKE_EXTRA_FLAGS_DESKTOP="

if "%WIX%" == ""                            set "WIX=C:/Program Files (x86)/WiX Toolset v3.14"
if "%WIX_SDK_PATH%" == ""                   set "WIX_SDK_PATH=C:/Program Files (x86)/WiX Toolset v3.14/SDK/VS2017"

Rem ********************************************************************************************
