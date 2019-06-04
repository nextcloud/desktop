@echo off
setlocal EnableDelayedExpansion
cls

echo "***** Build started. (%~nx0)"

Rem ******************************************************************************************
rem 			"Build everything"
Rem ******************************************************************************************

call "%~dp0/defaults.inc.bat" %1

Rem ******************************************************************************************

echo "* BUILD_TYPE=%BUILD_TYPE%"
echo "* BUILD_TARGETS=%BUILD_TARGETS%"

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html


Rem ******************************************************************************************
rem 			"check for required environment variables"
Rem ******************************************************************************************

call :testEnv PROJECT_PATH
call :testEnv BUILD_TYPE
call :testEnv BUILD_TARGETS
call :testEnv QT_PATH
call :testEnv OPENSSL_PATH
call :testEnv Png2Ico_EXECUTABLE
call :testEnv VCINSTALLDIR
call :testEnv WIN_GIT_PATH

if %ERRORLEVEL% neq 0 goto onError

Rem ******************************************************************************************
rem 			"qtkeychain"
Rem ******************************************************************************************

echo "***** build qtkeychain."
start "build-qtkeychain.bat %BUILD_TYPE%" /D "%PROJECT_PATH%/" /B /wait "%~dp0/build-qtkeychain.bat" %BUILD_TYPE%
if %ERRORLEVEL% neq 0 goto onError


Rem ******************************************************************************************
rem 			"zlib"
Rem ******************************************************************************************

echo "***** build zlib."
start "build-zlib.bat %BUILD_TYPE%" /D "%PROJECT_PATH%/" /B /wait "%~dp0/build-zlib.bat" %BUILD_TYPE%
if %ERRORLEVEL% neq 0 goto onError


Rem ******************************************************************************************
rem 			"desktop"
Rem ******************************************************************************************

echo "***** build desktop."
start "build-desktop.bat %BUILD_TYPE%" /D "%PROJECT_PATH%/" /B /wait "%~dp0/build-desktop.bat" %BUILD_TYPE%
if %ERRORLEVEL% neq 0 goto onError


Rem ******************************************************************************************
rem 			"collect files for the installer"
Rem ******************************************************************************************

echo "***** collect files for the installer."
start "build-installer-collect.bat %BUILD_TYPE%" /D "%PROJECT_PATH%/" /B /wait "%~dp0/build-installer-collect.bat" %BUILD_TYPE%
if %ERRORLEVEL% neq 0 goto onError


Rem ******************************************************************************************
rem 			"build the installer"
Rem ******************************************************************************************

if "%BUILD_INSTALLER%" == "0" (
    echo "** Don't build the installer (disabled by BUILD_INSTALLER)"
) else (
    echo "***** build the installer."
    start "build-installer-exe.bat %BUILD_TYPE%" /D "%PROJECT_PATH%/" /B /wait "%~dp0/build-installer-exe.bat" %BUILD_TYPE%
)
if %ERRORLEVEL% neq 0 goto onError

Rem Note: Signing and upload of the installer is triggered by NSIS. see: nextcloud.nsi


Rem ******************************************************************************************

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