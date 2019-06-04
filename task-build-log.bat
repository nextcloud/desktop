@echo off
call "%~dp0/defaults.inc.bat" %1

call "%~dp0/datetime.inc.bat"
set DATE_START=%_date% %_time%
set DATE_START_SANE=%_date%-%_time:~0,2%-%_time:~3,2%-%_time:~6,2%
set LOG_PATH=%PROJECT_PATH%/logs
set LOG_FILE=%LOG_PATH%/last-build-%DATE_START_SANE%-%BUILD_TYPE%.log

if not exist "%LOG_PATH%" mkdir "%LOG_PATH%"
echo --- START: %0 - %DATE_START% ---> "%LOG_FILE%"

REM set TEST_RUN=1
start "build.bat %BUILD_TYPE%" /D "%PROJECT_PATH%/" /B /wait "%~dp0/build.bat" %BUILD_TYPE% >> "%LOG_FILE%" 2>&1
set RESULT=%ERRORLEVEL%

call "%~dp0/datetime.inc.bat"
set DATE_END=%_date% %_time%
echo --- END: %0 - %DATE_END% --->> "%LOG_FILE%"

exit %RESULT%