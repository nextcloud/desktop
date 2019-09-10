Rem ******************************************************************************************
rem 			"common environment Variables"
Rem ******************************************************************************************

rem Release or Debug
set BUILD_TYPE=Release

if "%~1" == "Debug" (set BUILD_TYPE=%~1)

Rem ******************************************************************************************
rem Win64 or Win32, VS 2017 or 2019 - note that 2019 generator syntax excludes Arch type,
rem it's provided by the -A option (OR: CMAKE_GENERATOR_PLATFORM) instead.
rem QT msvc2017 is still the latest because it actually works for both (2019 compilery is binary compatible)
set BUILD_ARCH=Win64
if	"%VS_VERSION%" == "2017"		set CMAKE_GENERATOR=Visual Studio 15 2017
if	"%VS_VERSION%" == "2019"		set CMAKE_GENERATOR=Visual Studio 16 2019
								
set CMAKE_GENERATOR_PLATFORM=x64								
set QT_BIN_PATH=%QT_PATH%/msvc2017_64/bin

if "%~2" == "Win32" (set BUILD_ARCH=%~2)

if "%BUILD_ARCH%" == "Win32" (
    if	"%VS_VERSION%" == "2017"	(	
		set CMAKE_GENERATOR=Visual Studio 15 2017
	)
	if	"%VS_VERSION%" == "2019"	(
		set CMAKE_GENERATOR=Visual Studio 16 2019
	)
	set CMAKE_GENERATOR_PLATFORM=Win32
    set QT_BIN_PATH=%QT_PATH%/msvc2017/bin
)

set PATH=%QT_BIN_PATH%;%PATH%

Rem ******************************************************************************************