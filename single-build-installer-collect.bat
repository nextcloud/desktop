@echo off
setlocal EnableDelayedExpansion
cls

echo "*** Build: installer-collect (%~nx0)"

Rem ******************************************************************************************
rem 			"environment Variables"
Rem ******************************************************************************************

call "%~dp0/common.inc.bat" %1 %2

Rem ******************************************************************************************

set MY_REPO=%PROJECT_PATH%/desktop
set MY_BUILD_PATH=%MY_REPO%/build
set MY_INSTALL_PATH=%PROJECT_PATH%/install/%BUILD_TYPE%/%BUILD_ARCH%
set MY_QT_DEPLOYMENT_PATH=%MY_INSTALL_PATH%/qt-libs
set MY_COLLECT_PATH=%PROJECT_PATH%/collect/%BUILD_TYPE%/%BUILD_ARCH%

echo "* BUILD_TYPE=%BUILD_TYPE%"
echo "* BUILD_ARCH=%BUILD_ARCH%"
echo "* PROJECT_PATH=%PROJECT_PATH%"

echo "* WIN_GIT_PATH=%WIN_GIT_PATH%"
echo "* VCINSTALLDIR=%VCINSTALLDIR%"

echo "* OPENSSL_ROOT_DIR=%OPENSSL_ROOT_DIR%"
echo "* ZLIB_PATH=%ZLIB_PATH%"
echo "* EXTRA_DEPLOY_PATH=%EXTRA_DEPLOY_PATH%"

echo "* DLL_SUFFIX=%DLL_SUFFIX%"

echo "* MY_REPO=%MY_REPO%"
echo "* MY_BUILD_PATH=%MY_BUILD_PATH%"
echo "* MY_INSTALL_PATH=%MY_INSTALL_PATH%"
echo "* MY_QT_DEPLOYMENT_PATH=%MY_QT_DEPLOYMENT_PATH%"
echo "* MY_COLLECT_PATH=%MY_COLLECT_PATH%"

echo "* PATH=%PATH%"

echo "* USE_CODE_SIGNING=%USE_CODE_SIGNING%"

Rem ******************************************************************************************
rem 			"check for required environment variables"
Rem ******************************************************************************************

call :testEnv PROJECT_PATH
call :testEnv BUILD_TYPE
call :testEnv BUILD_ARCH
call :testEnv WIN_GIT_PATH
call :testEnv OPENSSL_ROOT_DIR
call :testEnv ZLIB_PATH
call :testEnv EXTRA_DEPLOY_PATH

if "%USE_CODE_SIGNING%" == "1" (
    call :testEnv VCINSTALLDIR
    call :testEnv APPLICATION_VENDOR
    call :testEnv P12_KEY
    call :testEnv P12_KEY_PASSWORD
    call :testEnv SIGN_FILE_DIGEST_ALG
    call :testEnv SIGN_TIMESTAMP_URL
    call :testEnv SIGN_TIMESTAMP_DIGEST_ALG
)

if %ERRORLEVEL% neq 0 goto onError

Rem ******************************************************************************************
rem 			"Test run?"
Rem ******************************************************************************************

if "%TEST_RUN%" == "1" (
    echo "** TEST RUN - exit."
    exit
)

Rem ******************************************************************************************
rem 			"clean up"
Rem ******************************************************************************************

echo "* Remove old installation files %MY_COLLECT_PATH% from previous build."
start "rm -rf" /B /wait rm -rf "%MY_COLLECT_PATH%/"*
if %ERRORLEVEL% neq 0 goto onError

Rem ******************************************************************************************
rem 			"collect dependencies"
Rem ******************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

Rem Output path for collected files
echo "* mkdir %MY_COLLECT_PATH%/shellext (recursive)."
start "mkdir collect" /D "%MY_INSTALL_PATH%/" /B /wait "%WIN_GIT_PATH%\usr\bin\mkdir.exe" -p "%MY_COLLECT_PATH%/shellext"
Rem Note: Force the use Git Bash's mkdir.exe, usually found in C:\Program Files\Git\usr\bin
if %ERRORLEVEL% neq 0 goto onError

Rem Qt dependencies
echo "* copy Qt libs (including qt5keychain.dll)."
start "copy Qt libs" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_INSTALL_PATH%/qt-libs/"* "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

Rem Desktop Client and resources
echo "* copy language files (i18n)."
start "copy i18n" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_INSTALL_PATH%/i18n" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

echo "* copy desktop client files (bin/)."
start "copy bin/" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_INSTALL_PATH%/bin/"* "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

echo "* copy ocsync.dll."
start "copy ocsync.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_INSTALL_PATH%/bin/nextcloud/ocsync.dll" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

Rem shell extension dll's for Explorer integration (status icons)
echo "* copy OCContextMenu.dll to %MY_COLLECT_PATH%/shellext/."
start "copy OCContextMenu.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_INSTALL_PATH%/bin/OCContextMenu.dll" "%MY_COLLECT_PATH%/shellext/"
if %ERRORLEVEL% neq 0 goto onError

echo "* copy OCContextMenu.dll to %MY_COLLECT_PATH%/shellext/."
start "copy OCOverlays.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_INSTALL_PATH%/bin/OCOverlays.dll" "%MY_COLLECT_PATH%/shellext/"
if %ERRORLEVEL% neq 0 goto onError

echo "* copy OCUtil.dll to %MY_COLLECT_PATH%/shellext/."
start "copy OCUtil.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_INSTALL_PATH%/bin/OCUtil.dll" "%MY_COLLECT_PATH%/shellext/"
if %ERRORLEVEL% neq 0 goto onError

Rem exclude system file list
echo "* copy sync-exclude.lst."
start "copy sync-exclude.lst" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_INSTALL_PATH%/config/Nextcloud/sync-exclude.lst" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

Rem icon (hi-res version created by png2ico, if unavailable use lo-res: %MY_REPO%/admin/win/nsi/installer.ico)
echo "* copy nextcloud.ico."
if exist "%MY_BUILD_PATH%/src/gui/Nextcloud.ico" (
    start "copy nextcloud.ico" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_BUILD_PATH%/src/gui/Nextcloud.ico" "%MY_COLLECT_PATH%/nextcloud.ico"
) else (
    echo "  NOT FOUND - try to copy installer.ico to nextcloud.ico"
    start "copy installer.ico to nextcloud.ico" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_REPO%/admin/win/nsi/installer.ico" "%MY_COLLECT_PATH%/nextcloud.ico"
)
if %ERRORLEVEL% neq 0 goto onError

Rem Qt config file for correct deployment
echo "* copy qt.conf."
start "copy qt.conf" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%MY_REPO%/admin/win/nsi/qt.conf" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

Rem OpenSSL's libcrypto: Be future-proof! ;)
echo "* get libcrypto's dll filename from %OPENSSL_ROOT_DIR%/bin/."
start "get libcrypto's dll filename" /D "%OPENSSL_ROOT_DIR%/bin/" /B /wait ls libcrypto*.dll > "%PROJECT_PATH%"/tmp
if %ERRORLEVEL% neq 0 goto onError
set /p LIBCRYPTO_DLL_FILENAME= < "%PROJECT_PATH%"\tmp
if %ERRORLEVEL% neq 0 goto onError
del "%PROJECT_PATH%"\tmp
echo "* LIBCRYPTO_DLL_FILENAME=%LIBCRYPTO_DLL_FILENAME%"

echo "* copy %OPENSSL_ROOT_DIR%/bin/%LIBCRYPTO_DLL_FILENAME%."
start "copy zlib.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%OPENSSL_ROOT_DIR%/bin/%LIBCRYPTO_DLL_FILENAME%" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

Rem The current Win**OpenSSL-1_1_1b is built with VC 2017 runtime dependencies,
Rem so we don't need to copy from there any more.
Rem However, if a future version of libcrypto requires a different VC runtime,
Rem also copy e.g.: %OPENSSL_ROOT_DIR%/bin/msvcr120.dll

Rem zlib
echo "* copy zlib%DLL_SUFFIX%.dll."
start "copy zlib%DLL_SUFFIX%.dll" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%ZLIB_PATH%/bin/zlib%DLL_SUFFIX%.dll" "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

Rem extra dll's and other resources (e.g.: libeay32.dll, ssleay32.dll)
echo "* copy extra resources (dll's, etc.) from %EXTRA_DEPLOY_PATH%/."
start "copy %EXTRA_DEPLOY_PATH%/" /D "%MY_COLLECT_PATH%/" /B /wait cp -af "%EXTRA_DEPLOY_PATH%/"* "%MY_COLLECT_PATH%/"
if %ERRORLEVEL% neq 0 goto onError

Rem ******************************************************************************************
rem 			"code signing"
Rem ******************************************************************************************

if "%USE_CODE_SIGNING%" == "0" (
    echo "** Don't sign: Code signing is disabled by USE_CODE_SIGNING"
) else (
    echo "** Calling vcvars64.bat to add signtool to the PATH:"
    call "%VCINSTALLDIR%\Auxiliary\Build\vcvars64.bat"
    if %ERRORLEVEL% neq 0 goto onError
    for %%i in (signtool.exe) do @set SIGNTOOL=%%~$PATH:i

    if "!SIGNTOOL!" == "" (
        echo "** Unable to find signtool.exe in the PATH."
        goto onError
    ) else (
        echo "** Found signtool.exe: !SIGNTOOL!"
    )

    echo "** Code signing begins:"

    for %%G in (
            "nextcloud/ocsync.dll"
            "shellext/OCContextMenu.dll"
            "shellext/OCOverlays.dll"
            "shellext/OCUtil.dll"
            "%LIBCRYPTO_DLL_FILENAME%"
            "libeay32.dll"
            "ssleay32.dll"
            "nextcloud.exe"
            "nextcloudcmd.exe"
            "nextcloudsync.dll"
            "OCContextMenu.dll"
            "OCOverlays.dll"
            "ocsync.dll"
            "OCUtil.dll"
            "qt5keychain.dll"
            "zlib%DLL_SUFFIX%.dll"
        ) do (
            start "sign %%~G" /D "%PROJECT_PATH%/" /B /wait %~dp0/sign.bat "%MY_COLLECT_PATH%/%%~G"

            if !ERRORLEVEL! neq 0 goto onError
        )
    
    echo "** Code signing ends."
)

Rem ******************************************************************************************

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