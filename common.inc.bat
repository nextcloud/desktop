Rem ******************************************************************************************
rem 			"common environment Variables"
Rem ******************************************************************************************

rem Release or Debug
set BUILD_TYPE=Release

if "%~1" == "Debug" (set BUILD_TYPE=%~1)

Rem ******************************************************************************************
rem Win64 or Win32
set BUILD_ARCH=Win64
set CMAKE_GENERATOR=Visual Studio 15 2017 Win64
set QT_BIN_PATH=%QT_PATH%/msvc2017_64/bin

if "%~2" == "Win32" (set BUILD_ARCH=%~2)

if "%BUILD_ARCH%" == "Win32" (
    set CMAKE_GENERATOR=Visual Studio 15 2017
    set QT_BIN_PATH=%QT_PATH%/msvc2017/bin
)

set PATH=%QT_BIN_PATH%;%PATH%

Rem ******************************************************************************************