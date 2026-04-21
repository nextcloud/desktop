@echo off
REM SPDX-FileCopyrightText: 2019 Michael Schuster
REM SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
REM SPDX-License-Identifier: GPL-2.0-or-later
setlocal EnableDelayedExpansion
cls

Rem ********************************************************************************************
rem     "MSI installer - build setup for Windows 64-bit or 32-bit"
Rem ********************************************************************************************

call "%~dp0/common.inc.bat" %1 %2 %CRAFT_TAG_DESKTOP%

Rem ********************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

echo "**** Build installer-msi for %BUILD_TYPE% %BUILD_ARCH% (%~nx0)."

Rem ********************************************************************************************

Rem MY_REPO is the Nextcloud desktop repository root.
set "MY_REPO=%DESKTOP_REPO_PATH%"
set "MY_BUILD_PATH=%MY_REPO%/build"
set "MY_INSTALL_PATH=%PROJECT_PATH%/install/%BUILD_TYPE%/%BUILD_ARCH%"
set "MY_COLLECT_PATH=%PROJECT_PATH%/collect/%BUILD_TYPE%/%BUILD_ARCH%"
set "MY_MSI_PATH=%MY_INSTALL_PATH%/msi"

echo "* BUILD_TYPE=%BUILD_TYPE%"
echo "* BUILD_ARCH=%BUILD_ARCH%"
echo "* PROJECT_PATH=%PROJECT_PATH%"

echo "* INSTALLER_OUTPUT_PATH=%INSTALLER_OUTPUT_PATH%"

echo "* Build date %BUILD_DATE%"
echo "* VERSION_SUFFIX %VERSION_SUFFIX%"
echo "* APPLICATION_VENDOR %APPLICATION_VENDOR%"
echo "* TAG_DESKTOP %TAG_DESKTOP%"
echo "* APPLICATION_NAME %APPLICATION_NAME%"

echo "* DESKTOP_REPO_PATH=%DESKTOP_REPO_PATH%"
echo "* MY_REPO=%MY_REPO%"
echo "* MY_BUILD_PATH=%MY_BUILD_PATH%"
echo "* MY_INSTALL_PATH=%MY_INSTALL_PATH%"
echo "* MY_COLLECT_PATH=%MY_COLLECT_PATH%"
echo "* MY_MSI_PATH=%MY_MSI_PATH%"
echo "* WIX=%WIX%"

echo "* PATH=%PATH%"

echo "* USE_CODE_SIGNING=%USE_CODE_SIGNING%"
echo "* UPLOAD_BUILD=%UPLOAD_BUILD%"

Rem ********************************************************************************************
rem     "check for required environment variables"
Rem ********************************************************************************************

call :testEnv PROJECT_PATH
call :testEnv INSTALLER_OUTPUT_PATH
call :testEnv BUILD_TYPE
call :testEnv BUILD_ARCH
call :testEnv BUILD_DATE
call :testEnv WIX

if "%USE_CODE_SIGNING%" == "1" (
    call :testEnv VCINSTALLDIR
    call :testEnv APPLICATION_VENDOR
    call :testEnv TAG_DESKTOP
    call :testEnv APPLICATION_NAME
    call :testEnv CERTIFICATE_FILENAME
    call :testEnv CERTIFICATE_CSP
    call :testEnv CERTIFICATE_KEY_CONTAINER_NAME
    call :testEnv CERTIFICATE_PASSWORD
    call :testEnv SIGN_FILE_DIGEST_ALG
    call :testEnv SIGN_TIMESTAMP_URL
    call :testEnv SIGN_TIMESTAMP_DIGEST_ALG
)

if %ERRORLEVEL% neq 0 goto onError

Rem ********************************************************************************************
rem     "Test run?"
Rem ********************************************************************************************

if "%TEST_RUN%" == "1" (
    echo "** TEST RUN - exit."
    exit
)

Rem ********************************************************************************************
rem     "build MSI installer"
Rem ********************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

Rem Create output directory for the Installer
if not exist "%INSTALLER_OUTPUT_PATH%" (
    echo "* Create output directory for the Installer: %INSTALLER_OUTPUT_PATH% (recursive)."
    start "mkdir %INSTALLER_OUTPUT_PATH%" /D "%PROJECT_PATH%/" /B /wait "%WIN_GIT_PATH%\usr\bin\mkdir.exe" -p "%INSTALLER_OUTPUT_PATH%"
)
if %ERRORLEVEL% neq 0 goto onError

if "%BUILD_ARCH%" == "Win32" (
    set "BITNESS=32"
) else (
    set "BITNESS=64"
)

Rem VC Environment Variables
echo "** Calling vcvars64.bat to get the VC env vars:"
call "%VCINSTALLDIR%\Auxiliary\Build\vcvars64.bat"

Rem ********************************************************************************************
rem     "code signing"
Rem ********************************************************************************************

if "%USE_CODE_SIGNING%" == "0" (
    echo "** Don't sign: Code signing is disabled by USE_CODE_SIGNING"
) else (
    echo "** Trying to find signtool in the PATH (VC env vars):"

    for %%i in (signtool.exe) do @set "SIGNTOOL=%%~$PATH:i"

    if "!SIGNTOOL!" == "" (
        echo "** Unable to find signtool.exe in the PATH."
        goto onError
    ) else (
        echo "** Found signtool.exe: !SIGNTOOL!"
    )

    echo "** Signing helper DLL:"

    for %%G in (
            "NCMsiHelper%BITNESS%.dll"
        ) do (
            start "sign %%~G" /D "%PROJECT_PATH%/" /B /wait %~dp0/sign.bat "%MY_MSI_PATH%/%%~G"

            if !ERRORLEVEL! neq 0 goto onError
        )

    echo "** Code signing ends."
)

REM WiSubStg.vbs is part of the Windows Installer SDK and available with vcvars64.bat
REM It will be used for embedding extra locales in the MSI.
echo ** Trying to find WiSubStg.vbs in PATH ^(VC env vars^)
for %%i in (WiSubStg.vbs) do @set "WISUBSTG=%%~$PATH:i"
if "!WISUBSTG!" == "" (
    echo ** Did not find WiSubStg.vbs -- the resulting MSI will be only in English
) else (
    echo ** Found WiSubStg.vbs: !WISUBSTG!

    REM WiLangId.vbs ^(also part of the Windows Installer SDK^) is used to update the
    REM language IDs used in the installer.
    echo ** Trying to find WiLangId.vbs in PATH ^(VC env vars^)
    for %%i in (WiLangId.vbs) do @set "WILANGID=%%~$PATH:i"
    if "!WILANGID!" == "" (
        echo ** Did not find WiLangId.vbs -- the resulting MSI will only be in English
    ) else (
        echo ** Found WiLangId.vbs: !WILANGID!
    )
)

echo "* Run MSI build script with parameter '%MY_COLLECT_PATH%' to create installer."
start "make-msi.bat" /D "%MY_MSI_PATH%" /B /wait call make-msi.bat "%MY_COLLECT_PATH%"
if %ERRORLEVEL% neq 0 goto onError

Rem Find MSI, get filename
for /f "delims=" %%a in ('dir /b "%MY_MSI_PATH%"\*.msi') do (
    set "MSI_FILENAME=%%a"
)

if "%USE_CODE_SIGNING%" == "0" (
    echo "** Don't sign: Code signing is disabled by USE_CODE_SIGNING"
) else (
    echo "** Signing %MSI_FILENAME%:"

    for %%G in (
            "%MSI_FILENAME%"
        ) do (
            start "sign %%~G" /D "%PROJECT_PATH%/" /B /wait %~dp0/sign.bat "%MY_MSI_PATH%/%%~G"

            if !ERRORLEVEL! neq 0 goto onError
        )

    echo "** Code signing ends."
)

echo "* Move %MSI_FILENAME% to '%INSTALLER_OUTPUT_PATH%'."
start "move msi" /D "%MY_MSI_PATH%" /B /wait mv "%MSI_FILENAME%" "%INSTALLER_OUTPUT_PATH%"/
if %ERRORLEVEL% neq 0 goto onError

if "%UPLOAD_BUILD%" == "0" (
    echo "** Don't upload: Uploading is disabled by UPLOAD_BUILD"
) else (
    echo "* Upload %MSI_FILENAME%."
    start "upload msi" /D "%PROJECT_PATH%" /B /wait %~dp0/upload.bat %INSTALLER_OUTPUT_PATH%/%MSI_FILENAME%
)

Rem ********************************************************************************************

echo "**** Finished Build: installer-msi %BUILD_TYPE% %BUILD_ARCH% (%~nx0)"
exit 0

:onError
echo "**** Build FAILED: installer-msi %BUILD_TYPE% %BUILD_ARCH% (%~nx0)"
if %ERRORLEVEL% neq 0 exit %ERRORLEVEL%
if !ERRORLEVEL! neq 0 exit !ERRORLEVEL!
exit 1

:testEnv
if "!%*!" == "" (
    echo "Missing environment variable: %*"
    exit /B 1
)
exit /B
