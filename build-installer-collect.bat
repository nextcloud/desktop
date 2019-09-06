@echo off
setlocal EnableDelayedExpansion

Rem ******************************************************************************************
rem 			"installer - collect files for Windows 64-bit and/or 32-bit"
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

    set OPENSSL_ROOT_DIR=%OPENSSL_PATH%/%%G
    set ZLIB_PATH=%PROJECT_PATH%/libs/%BUILD_TYPE%/zlib/%%G

    set EXTRA_DEPLOY_PATH=%PROJECT_PATH%/deploy-extra/%BUILD_TYPE%/%%G

    echo "**** collect installer files for %%G (%~nx0)."
    start "single-build-installer-collect.bat %BUILD_TYPE% %%G" /D "%PROJECT_PATH%/" /B /wait "%~dp0/single-build-installer-collect.bat" %BUILD_TYPE% %%G

    if !ERRORLEVEL! neq 0 exit !ERRORLEVEL!
)

Rem ******************************************************************************************

exit 0