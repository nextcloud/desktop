@echo off
setlocal EnableDelayedExpansion

Rem ******************************************************************************************
rem 			"zlib - Build for Windows 64-bit and 32-bit"
Rem ******************************************************************************************

call "%~dp0/defaults.inc.bat" %1

Rem ******************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

for %%G in (%BUILD_TARGETS%) do (
    echo "**** build zlib for %%G (%~nx0)."
    start "single-build-zlib.bat %BUILD_TYPE% %%G" /D "%PROJECT_PATH%/" /B /wait "%~dp0/single-build-zlib.bat" %BUILD_TYPE% %%G

    if !ERRORLEVEL! neq 0 exit !ERRORLEVEL!
)

Rem ******************************************************************************************

exit 0