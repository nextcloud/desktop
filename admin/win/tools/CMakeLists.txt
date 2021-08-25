cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)

if(CMAKE_SIZEOF_VOID_P MATCHES 4)
    set(BITNESS 32)
else()
    set(BITNESS 64)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    NCToolsShared
)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)
add_definitions(-DNDEBUG)
add_definitions(-D_WINDOWS)

# Get APIs from from Vista onwards.
add_definitions(-D_WIN32_WINNT=0x0601)
add_definitions(-DWINVER=0x0601)

# Use automatic overload for suitable CRT safe-functions
# See https://docs.microsoft.com/de-de/cpp/c-runtime-library/security-features-in-the-crt?view=vs-2019
add_definitions(-D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1)
# Also: Disable compiler warnings because we don't use Windows CRT safe-functions explicitly and don't intend to
# as this is a pure cross-platform source the only alternative would be a ton of ifdefs with calls to the _s version
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

# Optimize for size
set(COMPILER_FLAGS "/GL /O1 /sdl /Zc:inline /Oi /EHsc /nologo")
set(LINKER_FLAGS "/LTCG /OPT:REF /SUBSYSTEM:WINDOWS /NOLOGO")

# Enable DEP, ASLR and CFG
set(LINKER_FLAGS "${LINKER_FLAGS} /nxcompat /dynamicbase /guard:cf")

# x86 only: Enable SafeSEH
if(CMAKE_SIZEOF_VOID_P MATCHES 4)
    set(LINKER_FLAGS "${LINKER_FLAGS} /safeseh")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMPILER_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILER_FLAGS}")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINKER_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS}")

# Use static runtime for all subdirectories
foreach(buildType "" "_DEBUG" "_MINSIZEREL" "_RELEASE" "_RELWITHDEBINFO")
    string(REPLACE "/MD" "/MT" "CMAKE_CXX_FLAGS${buildType}" "${CMAKE_CXX_FLAGS${buildType}}")
endforeach()

add_subdirectory(NCToolsShared)

if(BUILD_WIN_MSI)
    add_subdirectory(NCMsiHelper)
endif()

if(BUILD_WIN_TOOLS)
    add_subdirectory(NCNavRemove)
endif()
