@echo off
setlocal EnableDelayedExpansion
cls

Rem ******************************************************************************************
rem 			"installer - build combined setup for Windows 64-bit and 32-bit"
Rem ******************************************************************************************

call "%~dp0/defaults.inc.bat" %1

Rem ******************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

echo "**** Build installer-exe for %BUILD_TYPE% (%~nx0)."

Rem ******************************************************************************************

set MY_REPO=%PROJECT_PATH%/desktop
set MY_BUILD_PATH=%MY_REPO%/build
set MY_INSTALL_PATH=%PROJECT_PATH%/install/%BUILD_TYPE%
set MY_QT_DEPLOYMENT_PATH=%MY_INSTALL_PATH%/qt-libs
set MY_COLLECT_PATH=%PROJECT_PATH%/collect/%BUILD_TYPE%

echo "* BUILD_TYPE=%BUILD_TYPE%"
echo "* PROJECT_PATH=%PROJECT_PATH%"

echo "* INSTALLER_OUTPUT_PATH=%INSTALLER_OUTPUT_PATH%"
echo "* NSIS_EXTRA_FLAGS=%NSIS_EXTRA_FLAGS%"

echo "* Build date %BUILD_DATE%"
echo "* VERSION_SUFFIX %VERSION_SUFFIX%"

echo "* MY_REPO=%MY_REPO%"
echo "* MY_BUILD_PATH=%MY_BUILD_PATH%"
echo "* MY_INSTALL_PATH=%MY_INSTALL_PATH%"
echo "* MY_QT_DEPLOYMENT_PATH=%MY_QT_DEPLOYMENT_PATH%"
echo "* MY_COLLECT_PATH=%MY_COLLECT_PATH%"

echo "* PATH=%PATH%"

Rem ******************************************************************************************
rem 			"check for required environment variables"
Rem ******************************************************************************************

call :testEnv PROJECT_PATH
call :testEnv INSTALLER_OUTPUT_PATH
call :testEnv BUILD_TYPE
call :testEnv BUILD_DATE

if %ERRORLEVEL% neq 0 goto onError

Rem ******************************************************************************************
rem 			"Test run?"
Rem ******************************************************************************************

if "%TEST_RUN%" == "1" (
    echo "** TEST RUN - exit."
    exit
)

Rem ******************************************************************************************
rem 			"build installer"
Rem ******************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

echo "* save git HEAD commit hash from repo %MY_REPO%/."
start "git rev-parse HEAD" /D "%MY_REPO%/" /B /wait git rev-parse HEAD > "%PROJECT_PATH%"/tmp
if %ERRORLEVEL% neq 0 goto onError
set /p GIT_REVISION= < "%PROJECT_PATH%"\tmp
if %ERRORLEVEL% neq 0 goto onError
del "%PROJECT_PATH%"\tmp

Rem Create output directory for the Installer
if not exist "%INSTALLER_OUTPUT_PATH%" (
    echo "* Create output directory for the Installer: %INSTALLER_OUTPUT_PATH% (recursive)."
    start "mkdir %INSTALLER_OUTPUT_PATH%" /D "%PROJECT_PATH%/" /B /wait "%WIN_GIT_PATH%\usr\bin\mkdir.exe" -p "%INSTALLER_OUTPUT_PATH%"
)
if %ERRORLEVEL% neq 0 goto onError

Rem Note: Keep the makensis /Defines in sync with nextcloud.nsi (IS_INNER_SIGN_UNINSTALLER) !
set CURRENT_PATH=%~dp0
echo "* Run NSIS script with parameters BUILD_TYPE=%BUILD_TYPE% and GIT_REVISION=%GIT_REVISION% to create installer."
start "NSIS" /D "%PROJECT_PATH%" /B /wait makensis.exe /DBUILD_TYPE=%BUILD_TYPE% /DMIRALL_VERSION_SUFFIX=%VERSION_SUFFIX% /DMIRALL_VERSION_BUILD=%BUILD_DATE% /DGIT_REVISION=%GIT_REVISION:~0,6% %NSIS_EXTRA_FLAGS% "%~dp0/nextcloud.nsi"
if %ERRORLEVEL% neq 0 goto onError

Rem ******************************************************************************************

echo "**** Finished Build: installer-exe %BUILD_TYPE% (GIT_REVISION=%GIT_REVISION%) (%~nx0)"
exit 0

:onError
echo "**** Build FAILED: installer-exe %BUILD_TYPE% (%~nx0)"
if %ERRORLEVEL% neq 0 exit %ERRORLEVEL%
if !ERRORLEVEL! neq 0 exit !ERRORLEVEL!
exit 1

:testEnv
if "!%*!" == "" (
    echo "Missing environment variable: %*"
    exit /B 1
)
exit /B