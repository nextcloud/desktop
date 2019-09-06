@echo off
setlocal EnableDelayedExpansion

Rem ******************************************************************************************
rem 			"desktop - Build for Windows 64-bit and/or 32-bit"
Rem ******************************************************************************************

call "%~dp0/defaults.inc.bat" %1

Rem ******************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

for %%G in (%BUILD_TARGETS%) do (
    if "%BUILD_TYPE%" == "Debug" (
        set DLL_SUFFIX=d
    ) else (
        set DLL_SUFFIX=
    )

    set QTKEYCHAIN_PATH=%PROJECT_PATH%/libs/%BUILD_TYPE%/qtkeychain/%%G
    set OPENSSL_ROOT_DIR=%OPENSSL_PATH%/%%G
    set ZLIB_PATH=%PROJECT_PATH%/libs/%BUILD_TYPE%/zlib/%%G

    set QTKEYCHAIN_INCLUDE_DIR=!QTKEYCHAIN_PATH!/include/qt5keychain
    set QTKEYCHAIN_LIBRARY=!QTKEYCHAIN_PATH!/lib/qt5keychain.lib
    set OPENSSL_INCLUDE_DIR=!OPENSSL_ROOT_DIR!/include
    set OPENSSL_LIBRARIES=!OPENSSL_ROOT_DIR!/lib
    set ZLIB_INCLUDE_DIR=!ZLIB_PATH!/include
    set ZLIB_LIBRARY=!ZLIB_PATH!/lib/zlib!DLL_SUFFIX!.lib

    echo "**** build desktop for %%G (%~nx0)."
    start "single-build-desktop.bat %BUILD_TYPE% %%G" /D "%PROJECT_PATH%/" /B /wait "%~dp0/single-build-desktop.bat" %BUILD_TYPE% %%G

    if !ERRORLEVEL! neq 0 exit !ERRORLEVEL!
)

Rem ******************************************************************************************

exit 0