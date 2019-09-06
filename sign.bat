@echo off
setlocal EnableDelayedExpansion
cls

Rem ******************************************************************************************
rem 			"sign a binaray file (exe, dll)"
Rem ******************************************************************************************

call "%~dp0/defaults.inc.bat"

Rem ******************************************************************************************

echo "*** Sign file: %~1%."

if "%1" == "" (
    echo "Missing parameter: Please specify file to sign"
    exit 1
)

Rem ******************************************************************************************

echo "* PROJECT_PATH=%PROJECT_PATH%"

echo "* SIGNTOOL=%SIGNTOOL%"
echo "* VCINSTALLDIR=%VCINSTALLDIR%"
echo "* APPLICATION_VENDOR=%APPLICATION_VENDOR%"
echo "* P12_KEY=%P12_KEY%"
echo "* SIGN_FILE_DIGEST_ALG=%SIGN_FILE_DIGEST_ALG%"
echo "* SIGN_TIMESTAMP_URL=%SIGN_TIMESTAMP_URL%"
echo "* SIGN_TIMESTAMP_DIGEST_ALG=%SIGN_TIMESTAMP_DIGEST_ALG%"
echo "* USE_CODE_SIGNING=%USE_CODE_SIGNING%"

echo "* PATH=%PATH%"

Rem ******************************************************************************************
rem 			"check for required environment variables"
Rem ******************************************************************************************

if "%USE_CODE_SIGNING%" == "0" (
    echo "** Abort sign: Code signing is disabled by USE_CODE_SIGNING"
    exit
)

call :testEnv PROJECT_PATH
call :testEnv APPLICATION_VENDOR
call :testEnv P12_KEY
call :testEnv P12_KEY_PASSWORD
call :testEnv SIGN_FILE_DIGEST_ALG
call :testEnv SIGN_TIMESTAMP_URL
call :testEnv SIGN_TIMESTAMP_DIGEST_ALG

if %ERRORLEVEL% neq 0 goto onError

Rem ******************************************************************************************
rem 			"try to find signtool, if not specified via environment"
Rem ******************************************************************************************

REM Note: vcvars is the official way to set the path for all the VC and Win SDK tools.
REM       signtool.exe resides in a SDK version specific directory, like:
REM         C:\Program Files (x86)\Windows Kits\10\bin\10.0.17763.0\x86\signtool.exe

if "%SIGNTOOL%" == "" (
    echo "** SIGNTOOL not set: trying to find via VCINSTALLDIR:"
    call :testEnv VCINSTALLDIR
    if %ERRORLEVEL% neq 0 goto onError

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
)
call :testEnv SIGNTOOL
if %ERRORLEVEL% neq 0 goto onError

Rem ******************************************************************************************
rem 			"sign"
Rem ******************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

echo "* Run signtool on file: %~1%."
start "signtool" /D "%PROJECT_PATH%" /B /wait "%SIGNTOOL%" sign /debug /v /n "%APPLICATION_VENDOR%" /tr "%SIGN_TIMESTAMP_URL%" /td %SIGN_TIMESTAMP_DIGEST_ALG% /fd %SIGN_FILE_DIGEST_ALG% /f "%P12_KEY%" /p "%P12_KEY_PASSWORD%" "%~1"
if %ERRORLEVEL% neq 0 goto onError

Rem ******************************************************************************************

echo "*** Finished sign file: %~1%"
exit 0

:onError
echo "*** Sign FAILED for file: %~1%"
if %ERRORLEVEL% neq 0 exit %ERRORLEVEL%
if !ERRORLEVEL! neq 0 exit !ERRORLEVEL!
exit 1

:testEnv
if "!%*!" == "" (
    echo "Missing environment variable: %*"
    exit /B 1
)
exit /B