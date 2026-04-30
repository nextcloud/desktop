@echo off
REM SPDX-FileCopyrightText: 2019 Michael Schuster
REM SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
REM SPDX-License-Identifier: GPL-2.0-or-later
setlocal EnableDelayedExpansion
cls

echo "*** Build: desktop (%~nx0)"

Rem ********************************************************************************************
rem     "environment Variables"
Rem ********************************************************************************************

call "%~dp0/common.inc.bat" %1 %2 %CRAFT_TAG_DESKTOP%

Rem ********************************************************************************************

if "%TAG%" == "" set TAG=%TAG_DESKTOP%

set "VERSION_SUFFIX=%VERSION_SUFFIX%"
if "%VERSION_SUFFIX%" == "" (
set "CMAKE_VERSION_SUFFIX=-DMIRALL_VERSION_SUFFIX=\"\""
) else (
set "CMAKE_VERSION_SUFFIX=-DMIRALL_VERSION_SUFFIX=%VERSION_SUFFIX%"
)

Rem MY_REPO is the Nextcloud desktop repository root.  When CHECKOUT_DESKTOP=0 (the default),
Rem the existing checkout at DESKTOP_REPO_PATH is used directly.
set MY_REPO=%DESKTOP_REPO_PATH%
set MY_BUILD_PATH=%MY_REPO%/build
set MY_INSTALL_PATH=%PROJECT_PATH%/install/%BUILD_TYPE%/%BUILD_ARCH%
set MY_QT_DEPLOYMENT_PATH=%MY_INSTALL_PATH%/qt-libs

echo "* APP_NAME=%APP_NAME%"
echo "* USE_BRANDING=%USE_BRANDING%"
echo "* BUILD_TYPE=%BUILD_TYPE%"
echo "* BUILD_ARCH=%BUILD_ARCH%"
echo "* CMAKE_GENERATOR=%CMAKE_GENERATOR%"
echo "* CMAKE_EXTRA_FLAGS_DESKTOP=%CMAKE_EXTRA_FLAGS_DESKTOP%"
echo "* PROJECT_PATH=%PROJECT_PATH%"

echo "* QT_PREFIX=%QT_PREFIX%"
echo "* QT_PATH=%QT_PATH%"
echo "* QT_BIN_PATH=%QT_BIN_PATH%"

echo "* VCINSTALLDIR=%VCINSTALLDIR%"
echo "* Png2Ico_EXECUTABLE=%Png2Ico_EXECUTABLE%"

echo "* Build date %BUILD_DATE%"
echo "* VERSION_SUFFIX %VERSION_SUFFIX%"
echo "* TAG %TAG%"
echo "* PULL_DESKTOP %PULL_DESKTOP%"
echo "* CHECKOUT_DESKTOP %CHECKOUT_DESKTOP%"
echo "* BUILD_UPDATER %BUILD_UPDATER%"
echo "* BUILD_INSTALLER_MSI %BUILD_INSTALLER_MSI%"

echo "* DESKTOP_REPO_PATH=%DESKTOP_REPO_PATH%"
echo "* MY_REPO=%MY_REPO%"
echo "* MY_BUILD_PATH=%MY_BUILD_PATH%"
echo "* MY_INSTALL_PATH=%MY_INSTALL_PATH%"
echo "* MY_QT_DEPLOYMENT_PATH=%MY_QT_DEPLOYMENT_PATH%"

echo "* WIX_SDK_PATH=%WIX_SDK_PATH%"

echo "* PATH=%PATH%"

Rem ********************************************************************************************
rem     "check for required environment variables"
Rem ********************************************************************************************

call :testEnv APP_NAME
call :testEnv PROJECT_PATH
call :testEnv BUILD_TYPE
call :testEnv BUILD_ARCH
call :testEnv CMAKE_GENERATOR
call :testEnv QT_PREFIX
call :testEnv QT_PATH
call :testEnv QT_BIN_PATH
call :testEnv VCINSTALLDIR
call :testEnv Png2Ico_EXECUTABLE
call :testEnv WIX_SDK_PATH
call :testEnv BUILD_DATE
call :testEnv BUILD_UPDATER
call :testEnv TAG

if %ERRORLEVEL% neq 0 goto onError

if "%BUILD_ARCH%" == "Win64" ( call "%VCINSTALLDIR%\Auxiliary\Build\vcvarsall.bat" x64 )
if "%BUILD_ARCH%" == "Win32" ( call "%VCINSTALLDIR%\Auxiliary\Build\vcvarsall.bat" amd64_x86 )

Rem ********************************************************************************************
rem     "Test run?"
Rem ********************************************************************************************

if "%TEST_RUN%" == "1" (
    echo "** TEST RUN - exit."
    exit
)

Rem ********************************************************************************************
rem     "clean up"
Rem ********************************************************************************************

echo "* Remove old installation files %MY_INSTALL_PATH% from previous build."
start "rm -rf" /B /wait rm -rf "%MY_INSTALL_PATH%/"*
if %ERRORLEVEL% neq 0 goto onError

echo "* Remove old dependencies files %MY_QT_DEPLOYMENT_PATH% from previous build."
start "rm -rf" /B /wait rm -rf "%MY_QT_DEPLOYMENT_PATH%/"*
if %ERRORLEVEL% neq 0 goto onError

echo "* Remove %MY_BUILD_PATH% from previous build."
start "rm -rf" /B /wait rm -rf "%MY_BUILD_PATH%/"*
if %ERRORLEVEL% neq 0 goto onError

Rem ********************************************************************************************
rem     "git pull, build, collect dependencies"
Rem ********************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

if "%PULL_DESKTOP%" == "1" (
    Rem Checkout master first to have it clean for git pull
    if "%CHECKOUT_DESKTOP%" == "1" (
        echo "* Remove %MY_REPO% from previous build."
        start "rm -rf" /B /wait rm -rf "%MY_REPO%/"
        if %ERRORLEVEL% neq 0 goto onError

        echo "* git clone %TAG% at %MY_REPO%/."
        start "git clone %TAG%" /B /wait git clone --depth=1 --branch=%TAG% https://github.com/nextcloud/desktop %MY_REPO%
    )
    if !ERRORLEVEL! neq 0 goto onError
) else (
    if "%CHECKOUT_DESKTOP%" == "1" (
        echo "* Remove %MY_REPO% from previous build."
        start "rm -rf" /B /wait rm -rf "%MY_REPO%/"*
        if %ERRORLEVEL% neq 0 goto onError

        echo "* git checkout %TAG% at %MY_REPO%/."
        start "git checkout %TAG%" /B /wait git clone --depth=1 --branch=%TAG% https://github.com/nextcloud/desktop %MY_REPO%
        if !ERRORLEVEL! neq 0 goto onError
    )
    if %ERRORLEVEL% neq 0 goto onError
)
if %ERRORLEVEL% neq 0 goto onError

echo "* Create desktop build directory"
start "mkdir %MY_BUILD_PATH%" /D "%PROJECT_PATH%/" /B /wait "%WIN_GIT_PATH%\usr\bin\mkdir.exe" -p "%MY_BUILD_PATH%"
if %ERRORLEVEL% neq 0 goto onError

echo "* save git HEAD commit hash from repo %MY_REPO%/."
start "git rev-parse HEAD" /D "%MY_REPO%/" /B /wait git rev-parse HEAD > "%PROJECT_PATH%"/tmp
if %ERRORLEVEL% neq 0 goto onError
set /p GIT_REVISION= < "%PROJECT_PATH%"\tmp
if %ERRORLEVEL% neq 0 goto onError
del "%PROJECT_PATH%"\tmp

echo "* Run cmake with CMAKE_INSTALL_PREFIX and CMAKE_BUILD_TYPE set at %MY_BUILD_PATH%."
echo "cmake -G%CMAKE_GENERATOR% .. %CMAKE_VERSION_SUFFIX% -DBUILD_TESTING=OFF -DWITH_CRASHREPORTER=OFF -DBUILD_UPDATER=%BUILD_UPDATER% -DBUILD_WIN_MSI=%BUILD_INSTALLER_MSI% -DMIRALL_VERSION_BUILD=%BUILD_DATE% -DCMAKE_INSTALL_PREFIX=%MY_INSTALL_PATH% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_PREFIX_PATH=%CRAFT_PATH% -DPng2Ico_EXECUTABLE=%Png2Ico_EXECUTABLE% %CMAKE_EXTRA_FLAGS_DESKTOP% -DWIX_SDK_PATH=%WIX_SDK_PATH%"
start "cmake.." /D "%MY_BUILD_PATH%" /B /wait cmake "-G%CMAKE_GENERATOR%" .. -DMIRALL_VERSION_SUFFIX="%VERSION_SUFFIX%" -DBUILD_TESTING=OFF -DWITH_CRASHREPORTER=OFF -DBUILD_UPDATER=%BUILD_UPDATER% -DBUILD_WIN_MSI=%BUILD_INSTALLER_MSI% -DMIRALL_VERSION_BUILD="%BUILD_DATE%" -DCMAKE_INSTALL_PREFIX="%MY_INSTALL_PATH%" -DCMAKE_BUILD_TYPE="%BUILD_TYPE%" -DCMAKE_PREFIX_PATH=%CRAFT_PATH% -DPng2Ico_EXECUTABLE="%Png2Ico_EXECUTABLE%" %CMAKE_EXTRA_FLAGS_DESKTOP% "-DWIX_SDK_PATH=%WIX_SDK_PATH%"
if %ERRORLEVEL% neq 0 goto onError

echo "* Run cmake to compile and install."
start "cmake build" /D "%MY_BUILD_PATH%" /B /wait cmake --build . --config %BUILD_TYPE% --target install
if %ERRORLEVEL% neq 0 goto onError

if "%BUILD_TYPE%" == "Debug" (
    set WINDEPLOYQT_BUILD_TYPE=debug
) else (
    set WINDEPLOYQT_BUILD_TYPE=release
)
echo "* Run windeployqt to collect all %APP_NAME%.exe dependencies and output it to %MY_QT_DEPLOYMENT_PATH%/."
start "windeployqt" /B /wait %QT_BIN_PATH%/windeployqt.exe --%WINDEPLOYQT_BUILD_TYPE% --compiler-runtime "%MY_INSTALL_PATH%/bin/%APP_NAME%.exe" --dir "%MY_QT_DEPLOYMENT_PATH%/" --qmldir "%MY_REPO%/src/gui" -websockets
if %ERRORLEVEL% neq 0 goto onError

Rem ********************************************************************************************

echo "*** Finished Build: desktop %BUILD_TYPE% %BUILD_ARCH% (GIT_REVISION=%GIT_REVISION%) (%~nx0)"
exit 0

:onError
echo "*** Build FAILED: desktop %BUILD_TYPE% %BUILD_ARCH% (%~nx0)"
if %ERRORLEVEL% neq 0 exit %ERRORLEVEL%
if !ERRORLEVEL! neq 0 exit !ERRORLEVEL!
exit 1

:testEnv
if "!%*!" == "" (
    echo "Missing environment variable: %*"
    exit /B 1
)
exit /B
