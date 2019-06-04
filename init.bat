@echo off
setlocal EnableDelayedExpansion
cls

echo "***** Init started. (%~nx0)"

Rem ******************************************************************************************
rem 			"init: git clone everything, create folders"
Rem ******************************************************************************************

call "%~dp0/defaults.inc.bat"

Rem ******************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

echo "* BUILD_TARGETS=%BUILD_TARGETS%"
echo "* PROJECT_PATH=%PROJECT_PATH%"

echo "* WIN_GIT_PATH=%WIN_GIT_PATH%"
echo "* INSTALLER_OUTPUT_PATH=%INSTALLER_OUTPUT_PATH%"

Rem ******************************************************************************************
rem 			"check for required environment variables"
Rem ******************************************************************************************

call :testEnv PROJECT_PATH
call :testEnv BUILD_TARGETS
call :testEnv WIN_GIT_PATH

if "%BUILD_INSTALLER%" == "1" (
    call :testEnv INSTALLER_OUTPUT_PATH
)

if %ERRORLEVEL% neq 0 goto onError

Rem ******************************************************************************************

Rem Create directory for extra deployment resources (EXTRA_DEPLOY_PATH)
for %%G in (Debug, Release) do (
    for %%H in (%BUILD_TARGETS%) do (
        set EXTRA_DEPLOY_PATH=%PROJECT_PATH%/deploy-extra/%%G/%%H

        echo "* Create extra deployment directory: !EXTRA_DEPLOY_PATH! (recursive)."
        start "mkdir !EXTRA_DEPLOY_PATH!" /D "%PROJECT_PATH%/" /B /wait "%WIN_GIT_PATH%\usr\bin\mkdir.exe" -p "!EXTRA_DEPLOY_PATH!"
        Rem Note: Force the use Git Bash's mkdir.exe, usually found in C:\Program Files\Git\usr\bin
        Rem This also creates PROJECT_PATH if necessary.

        if !ERRORLEVEL! neq 0 exit !ERRORLEVEL!
    )
)

Rem Create output directory for the Installer
if "%BUILD_INSTALLER%" == "1" (
    echo "* Create output directory for the Installer: %INSTALLER_OUTPUT_PATH% (recursive)."
    start "mkdir %INSTALLER_OUTPUT_PATH%" /D "%PROJECT_PATH%/" /B /wait "%WIN_GIT_PATH%\usr\bin\mkdir.exe" -p "%INSTALLER_OUTPUT_PATH%"
)
if %ERRORLEVEL% neq 0 goto onError


Rem ******************************************************************************************
rem 			"qtkeychain"
Rem ******************************************************************************************

echo "* git clone qtkeychain."
start "git clone qtkeychain" /D "%PROJECT_PATH%/" /B /wait git clone https://github.com/frankosterfeld/qtkeychain
if %ERRORLEVEL% neq 0 goto onError

echo "* Create qtkeychain build directory (recursive)."
start "mkdir qtkeychain/build" /D "%PROJECT_PATH%/" /B /wait "%WIN_GIT_PATH%\usr\bin\mkdir.exe" -p "%PROJECT_PATH%/qtkeychain/build"
if %ERRORLEVEL% neq 0 goto onError


Rem ******************************************************************************************
rem 			"zlib"
Rem ******************************************************************************************

echo "* git clone zlib."
start "git clone zlib" /D "%PROJECT_PATH%/" /B /wait git clone https://github.com/madler/zlib.git
if %ERRORLEVEL% neq 0 goto onError

echo "* Create zlib build directory (recursive)."
start "mkdir zlib/build" /D "%PROJECT_PATH%/" /B /wait "%WIN_GIT_PATH%\usr\bin\mkdir.exe" -p "%PROJECT_PATH%/zlib/build"
if %ERRORLEVEL% neq 0 goto onError


Rem ******************************************************************************************
rem 			"nextcloud/desktop"
Rem ******************************************************************************************

echo "* git clone nextcloud/desktop."
start "git clone nextcloud/desktop" /D "%PROJECT_PATH%/" /B /wait git clone git://github.com/nextcloud/desktop.git
if %ERRORLEVEL% neq 0 goto onError

echo "* Create nextcloud/desktop build directory (recursive)."
start "mkdir desktop/build" /D "%PROJECT_PATH%/" /B /wait "%WIN_GIT_PATH%\usr\bin\mkdir.exe" -p "%PROJECT_PATH%/desktop/build"
if %ERRORLEVEL% neq 0 goto onError


Rem ******************************************************************************************

echo "***** Init finished. (%~nx0)"
exit 0

:onError
echo "***** Init FAILED! (%~nx0)"
if %ERRORLEVEL% neq 0 exit %ERRORLEVEL%
if !ERRORLEVEL! neq 0 exit !ERRORLEVEL!
exit 1

:testEnv
if "!%*!" == "" (
    echo "Missing environment variable: %*"
    exit /B 1
)
exit /B