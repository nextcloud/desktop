@echo off
REM SPDX-FileCopyrightText: 2019 Michael Schuster
REM SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
REM SPDX-License-Identifier: GPL-2.0-or-later
setlocal EnableDelayedExpansion
cls

echo "*** Build: installer-collect (%~nx0)"

Rem ********************************************************************************************
rem     "environment Variables"
Rem ********************************************************************************************

call "%~dp0/common.inc.bat" %1 %2 %CRAFT_TAG_DESKTOP%

Rem ********************************************************************************************

Rem MY_REPO is the Nextcloud desktop repository root, used to locate qt.conf and the app icon.
set "MY_REPO=%DESKTOP_REPO_PATH%"
set "MY_BUILD_PATH=%MY_REPO%/build"
set "MY_INSTALL_PATH=%PROJECT_PATH%/install/%BUILD_TYPE%/%BUILD_ARCH%"
set "MY_QT_DEPLOYMENT_PATH=%MY_INSTALL_PATH%/qt-libs"
set "MY_COLLECT_PATH=%PROJECT_PATH%/collect/%BUILD_TYPE%/%BUILD_ARCH%"

echo "* APP_NAME=%APP_NAME%"
echo "* APP_NAME_SANITIZED=%APP_NAME_SANITIZED%"
echo "* USE_BRANDING=%USE_BRANDING%"
echo "* BUILD_TYPE=%BUILD_TYPE%"
echo "* BUILD_ARCH=%BUILD_ARCH%"
echo "* PROJECT_PATH=%PROJECT_PATH%"

echo "* Build date %BUILD_DATE%"
echo "* VERSION_SUFFIX %VERSION_SUFFIX%"
echo "* APPLICATION_VENDOR %APPLICATION_VENDOR%"
echo "* TAG_DESKTOP %TAG_DESKTOP%"
echo "* APPLICATION_NAME %APPLICATION_NAME%"

echo "* WIN_GIT_PATH=%WIN_GIT_PATH%"
echo "* VCINSTALLDIR=%VCINSTALLDIR%"

echo "* EXTRA_DEPLOY_PATH=%EXTRA_DEPLOY_PATH%"

echo "* DLL_SUFFIX=%DLL_SUFFIX%"

echo "* DESKTOP_REPO_PATH=%DESKTOP_REPO_PATH%"
echo "* MY_REPO=%MY_REPO%"
echo "* MY_BUILD_PATH=%MY_BUILD_PATH%"
echo "* MY_INSTALL_PATH=%MY_INSTALL_PATH%"
echo "* MY_QT_DEPLOYMENT_PATH=%MY_QT_DEPLOYMENT_PATH%"
echo "* MY_COLLECT_PATH=%MY_COLLECT_PATH%"

echo "* PATH=%PATH%"

echo "* USE_CODE_SIGNING=%USE_CODE_SIGNING%"

Rem ********************************************************************************************
rem     "check for required environment variables"
Rem ********************************************************************************************

call :testEnv APP_NAME
call :testEnv APP_NAME_SANITIZED
call :testEnv PROJECT_PATH
call :testEnv BUILD_TYPE
call :testEnv BUILD_ARCH
call :testEnv WIN_GIT_PATH
call :testEnv EXTRA_DEPLOY_PATH

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
rem     "clean up"
Rem ********************************************************************************************

echo "* Remove old installation files %MY_COLLECT_PATH% from previous build."
start "rm -rf" /B /wait rm -rf "%MY_COLLECT_PATH%/"*
if %ERRORLEVEL% neq 0 goto onError

Rem ********************************************************************************************
rem     "collect dependencies"
Rem ********************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

Rem Output path for collected files
echo "* mkdir %MY_COLLECT_PATH%"
start "mkdir collect" /D "%MY_INSTALL_PATH%/" /B /wait "%WIN_GIT_PATH%\usr\bin\mkdir.exe" -p "%MY_COLLECT_PATH%"
Rem Note: Force the use Git Bash's mkdir.exe, usually found in C:\Program Files\Git\usr\bin
if %ERRORLEVEL% neq 0 goto onError

Rem Desktop Client and resources
echo "* copy language files (i18n)."
start "copy i18n" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_INSTALL_PATH%/i18n" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

echo "* copy desktop client files (bin/)."
start "copy bin/" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_INSTALL_PATH%/bin/"* "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

echo "* copy %APP_NAME_SANITIZED%_csync.dll."
start "copy %APP_NAME_SANITIZED%_csync.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_INSTALL_PATH%/bin/%APP_NAME_SANITIZED%_csync.dll" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

Rem exclude system file list
echo "* copy sync-exclude.lst."
start "copy sync-exclude.lst" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_INSTALL_PATH%/config/%APP_NAME%/sync-exclude.lst" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

Rem icon (hi-res version created by png2ico, if unavailable use lo-res: admin/win/nsi/installer.ico)
echo "* copy %APP_NAME_SANITIZED%.ico."
if exist "%MY_BUILD_PATH%/src/gui/%APP_NAME_SANITIZED%.ico" (
    start "copy %APP_NAME_SANITIZED%.ico" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_BUILD_PATH%/src/gui/%APP_NAME_SANITIZED%.ico" "%MY_COLLECT_PATH%/%APP_NAME_SANITIZED%.ico"
) else (
    echo "  NOT FOUND - try to copy installer.ico to %APP_NAME_SANITIZED%.ico"
    start "copy installer.ico to %APP_NAME_SANITIZED%.ico" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_REPO%/admin/win/nsi/installer.ico" "%MY_COLLECT_PATH%/%APP_NAME_SANITIZED%.ico"
)
if %ERRORLEVEL% neq 0 goto onError

echo "* run windeployqt "%MY_COLLECT_PATH%/%APP_NAME_SANITIZED%.exe."
start "run windeployqt" /D "%MY_COLLECT_PATH%/" /B /wait windeployqt %PDB_OPTION% --compiler-runtime --qmldir "%MY_REPO%\src" --release --force --verbose 2 "%MY_COLLECT_PATH%/%APP_NAME_SANITIZED%.exe" "%MY_COLLECT_PATH%/%APP_NAME_SANITIZED%_csync.dll" "%MY_COLLECT_PATH%/%APP_NAME_SANITIZED%cmd.exe" "%MY_COLLECT_PATH%/%APP_NAME_SANITIZED%sync.dll"
if %ERRORLEVEL% neq 0 goto onError

Rem Remove Qt bearer plugins, they seem to cause issues on Windows
echo "* remove Qt bearer plugins"
start "remove Qt bearer plugins" /D "%MY_COLLECT_PATH%/" /B /wait rm -rf "%MY_COLLECT_PATH%/bearer"
if %ERRORLEVEL% neq 0 goto onError

echo "* copy Qt dependencies."
start "copy bin/" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%CRAFT_PATH%/bin/freetype%DLL_SUFFIX%.dll" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

Rem Qt config file for correct deployment
echo "* copy qt.conf."
start "copy qt.conf" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_REPO%/admin/win/nsi/qt.conf" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

Rem OpenSSL's libcrypto: Be future-proof! ;)
echo "* get libcrypto's dll filename from %CRAFT_PATH%/bin/."
start "get libcrypto's dll filename" /D "%CRAFT_PATH%/bin/" /B /wait ls libcrypto-3*.dll > "%PROJECT_PATH%"/tmp
if %ERRORLEVEL% neq 0 goto onError
set /p LIBCRYPTO_DLL_FILENAME= < "%PROJECT_PATH%"\tmp
if %ERRORLEVEL% neq 0 goto onError
del "%PROJECT_PATH%"\tmp
echo "* LIBCRYPTO_DLL_FILENAME=%LIBCRYPTO_DLL_FILENAME%"

echo "* copy %CRAFT_PATH%/bin/%LIBCRYPTO_DLL_FILENAME%."
start "copy %LIBCRYPTO_DLL_FILENAME%" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%CRAFT_PATH%/bin/%LIBCRYPTO_DLL_FILENAME%" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

Rem OpenSSL's libssl
echo "* get libssl's dll filename from %CRAFT_PATH%/bin/."
start "get libssl's dll filename" /D "%CRAFT_PATH%/bin/" /B /wait ls libssl-3*.dll > "%PROJECT_PATH%"/tmp
if %ERRORLEVEL% neq 0 goto onError
set /p LIBSSL_DLL_FILENAME= < "%PROJECT_PATH%"\tmp
if %ERRORLEVEL% neq 0 goto onError
del "%PROJECT_PATH%"\tmp
echo "* LIBSSL_DLL_FILENAME=%LIBSSL_DLL_FILENAME%"

echo "* copy %CRAFT_PATH%/bin/%LIBSSL_DLL_FILENAME%."
start "copy %LIBSSL_DLL_FILENAME%" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%CRAFT_PATH%/bin/%LIBSSL_DLL_FILENAME%" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

echo "* copy libp11.dll"
start "copy libp11.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%CRAFT_PATH%/bin/libp11.dll" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto

echo "* copy kdsingleapplication-qt6.dll"
start "copy kdsingleapplication-qt6.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%CRAFT_PATH%/bin/kdsingleapplication-qt6.dll" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto

Rem zlib
echo "* copy zlib1%DLL_SUFFIX%.dll."
start "copy zlib1%DLL_SUFFIX%.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%CRAFT_PATH%/bin/zlib1%DLL_SUFFIX%.dll" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto

echo "* copy KArchive files (bin/)."
start "copy bin/" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%CRAFT_PATH%/bin/KF6Archive%DLL_SUFFIX%.dll"* "%CRAFT_PATH%/bin/bz2%DLL_SUFFIX%.dll" "%CRAFT_PATH%/bin/liblzma%DLL_SUFFIX%.dll" "%CRAFT_PATH%/bin/zstd%DLL_SUFFIX%.dll" "%CRAFT_PATH%/bin/pcre2-16%DLL_SUFFIX%.dll" "%CRAFT_PATH%/bin/libpng16%DLL_SUFFIX%.dll" "%CRAFT_PATH%/bin/harfbuzz%DLL_SUFFIX%.dll" "%CRAFT_PATH%/bin/jpeg62%DLL_SUFFIX%.dll" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

echo "* copy brotlicommon.dll"
start "copy brotlicommon.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%CRAFT_PATH%/bin/brotlicommon.dll" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto

echo "* copy brotlidec.dll"
start "copy brotlidec.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%CRAFT_PATH%/bin/brotlidec.dll" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto

Rem sqlite
echo "* copy libsqlite.dll"
start "copy libsqlite.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%CRAFT_PATH%/bin/libsqlite.dll" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto

echo "* copy Qt OpenSSL backend plugin"
start "copy qopensslbackend.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%CRAFT_PATH%/plugins/tls/qopensslbackend.dll" "%MY_COLLECT_PATH%/tls"
if %ERRORLEVEL% neq 0 goto

echo "* copy b2-1.dll"
start "copy b2-1.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%CRAFT_PATH%/bin/b2-1.dll" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto

Rem deploy-extra: optional extra dll's and other resources
echo "* copy optional extra resources (dll's, etc.) from %EXTRA_DEPLOY_PATH%/."
( dir /b /a "%EXTRA_DEPLOY_PATH%" | findstr . ) > nul && (
    start "copy %EXTRA_DEPLOY_PATH%/" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%EXTRA_DEPLOY_PATH%/"* "%MY_COLLECT_PATH%/"
) || (
    echo "* NOTE: nothing found to copy from %EXTRA_DEPLOY_PATH%/."
    goto skipDeployExtra
)
if %ERRORLEVEL% neq 0 goto onError
:skipDeployExtra

Rem VC Environment Variables
echo "** Calling vcvars64.bat to get the VC env vars:"
call "%VCINSTALLDIR%\Auxiliary\Build\vcvars64.bat"

Rem VC Redist
echo "* copy VC Redist Runtime DLLs from %VCToolsRedistDir%/."
if "%BUILD_ARCH%" == "Win64" (
    start "copy VC Redist x64" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%VCToolsRedistDir%/x64/Microsoft.VC143.CRT/"* "%MY_COLLECT_PATH%/"
    start "copy VC OMP Redist x64" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%VCToolsRedistDir%/x64/Microsoft.VC143.OpenMP/"* "%MY_COLLECT_PATH%/"
) else (
    start "copy VC Redist x86" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%VCToolsRedistDir%/x86/Microsoft.VC143.CRT/"* "%MY_COLLECT_PATH%/"
    start "copy VC OMP Redist x86" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%VCToolsRedistDir%/x86/Microsoft.VC143.OpenMP/"* "%MY_COLLECT_PATH%/"
)
if %ERRORLEVEL% neq 0 goto onError

echo "* remove VC Redist installer(s) from %MY_COLLECT_PATH%/."
start "remove vc*redist*.exe" /D "%MY_COLLECT_PATH%/" /B /wait rm -f "%MY_COLLECT_PATH%"/vc*redist*.exe

Rem ********************************************************************************************
rem     "code signing"
Rem ********************************************************************************************

if "%USE_CODE_SIGNING%" == "0" (
    echo "** Don't sign: Code signing is disabled by USE_CODE_SIGNING"
) else (
    echo "** Trying to find signtool in the PATH (VC env vars):"

    for %%i in (signtool.exe) do @set SIGNTOOL=%%~$PATH:i

    if "!SIGNTOOL!" == "" (
        echo "** Unable to find signtool.exe in the PATH."
        goto onError
    ) else (
        echo "** Found signtool.exe: !SIGNTOOL!"
    )

    echo "** Code signing begins:"

    for %%G in (
            "NCContextMenu.dll"
            "NCOverlays.dll"
            "%APP_NAME_SANITIZED%.exe"
            "%APP_NAME_SANITIZED%cmd.exe"
            "%APP_NAME_SANITIZED%sync.dll"
            "%APP_NAME_SANITIZED%_csync.dll"
            "qt6keychain%DLL_SUFFIX%.dll"
            "%LIBCRYPTO_DLL_FILENAME%"
            "%LIBSSL_DLL_FILENAME%"
            "zlib1%DLL_SUFFIX%.dll"
        ) do (
            start "sign %%~G" /D "%PROJECT_PATH%/" /B /wait %~dp0/sign.bat "%MY_COLLECT_PATH%/%%~G"

            if !ERRORLEVEL! neq 0 goto onError
        )

    echo "** Code signing ends."
)

Rem ********************************************************************************************

echo "*** Finished Build: installer-collect %BUILD_TYPE% %BUILD_ARCH% (%~nx0)"
exit 0

:onError
echo "*** Build FAILED: installer-collect %BUILD_TYPE% %BUILD_ARCH% (%~nx0)"
if %ERRORLEVEL% neq 0 exit %ERRORLEVEL%
if !ERRORLEVEL! neq 0 exit !ERRORLEVEL!
exit 1

:testEnv
if "!%*!" == "" (
    echo "Missing environment variable: %*"
    exit /B 1
)
exit /B
