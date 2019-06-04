@echo off
setlocal EnableDelayedExpansion
cls

Rem ******************************************************************************************
rem 			"upload daily build installer-exe"
Rem ******************************************************************************************

call "%~dp0/defaults.inc.bat"

Rem ******************************************************************************************

echo "*** Upload file: %~1%."

if "%1" == "" (
    echo "Missing parameter: Please specify file to upload (in quotes)"
    exit 1
)

Rem ******************************************************************************************

echo "* PROJECT_PATH=%PROJECT_PATH%"

echo "* SFTP_PATH=%SFTP_PATH%"
echo "* SFTP_SERVER=%SFTP_SERVER%"
echo "* SFTP_USER=%SFTP_USER%"
echo "* UPLOAD_BUILD=%UPLOAD_BUILD%"

echo "* PATH=%PATH%"

Rem ******************************************************************************************
rem 			"check for required environment variables"
Rem ******************************************************************************************

call :testEnv PROJECT_PATH
call :testEnv SFTP_PATH
call :testEnv SFTP_SERVER
call :testEnv SFTP_USER

if %ERRORLEVEL% neq 0 goto onError

Rem ******************************************************************************************
rem 			"upload"
Rem ******************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

if "%UPLOAD_BUILD%" == "0" (
    echo "** Abort upload: Uploading is disabled by UPLOAD_BUILD"
    exit
)

set PATHTOEXEFILE=%1
set EXEFILENAME=%~nx1
set SSH_KEYFILE=%USERPROFILE%\.ssh\id_rsa

echo "* Upload installer."
start "upload" /D "%PROJECT_PATH%" /B /wait scp -i "%SSH_KEYFILE%" "%PATHTOEXEFILE%" %SFTP_USER%@%SFTP_SERVER%:"%SFTP_PATH%"/"%EXEFILENAME%" > "%PROJECT_PATH%\last_daily_upload.log" 2>&1
if %ERRORLEVEL% neq 0 goto onError

Rem ******************************************************************************************

echo "*** Finished upload file: %~1%"
exit %ERRORLEVEL%

:onError
echo "*** Upload FAILED for file: %~1%"
exit %ERRORLEVEL%

:testEnv
if "!%*!" == "" (
    echo "Missing environment variable: %*"
    exit /B 1
)
exit /B