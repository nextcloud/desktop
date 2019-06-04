@echo off
setlocal EnableDelayedExpansion

Rem ******************************************************************************************
rem 			"qtkeychain - Build for Windows 64-bit and 32-bit"
Rem ******************************************************************************************

call "%~dp0/defaults.inc.bat" %1

Rem ******************************************************************************************

rem Reference: https://ss64.com/nt/setlocal.html
rem Reference: https://ss64.com/nt/start.html

for %%G in (%BUILD_TARGETS%) do (
    echo "**** build qtkeychain for %%G (%~nx0)."
    start "single-build-qtkeychain.bat %BUILD_TYPE% %%G" /D "%PROJECT_PATH%/" /B /wait "%~dp0/single-build-qtkeychain.bat" %BUILD_TYPE% %%G

    if !ERRORLEVEL! neq 0 exit !ERRORLEVEL!
)

Rem ******************************************************************************************

exit 0