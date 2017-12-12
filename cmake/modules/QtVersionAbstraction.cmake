# (c) 2014 Copyright ownCloud GmbH
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING* file.

include (MacroOptionalFindPackage)
include (MacroLogFeature)

find_package(Qt5Core REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5Concurrent REQUIRED)
if(UNIT_TESTING)
    find_package(Qt5Test REQUIRED)
endif()

if(NOT TOKEN_AUTH_ONLY)
    find_package(Qt5Widgets REQUIRED)
    if(APPLE)
        find_package(Qt5MacExtras REQUIRED)
    endif(APPLE)

    if(NOT NO_SHIBBOLETH)
        find_package(Qt5WebKitWidgets)
        find_package(Qt5WebKit)
        if(NOT Qt5WebKitWidgets_FOUND)
            message(FATAL_ERROR "Qt5WebKit required for Shibboleth. Use -DNO_SHIBBOLETH=1 to disable it.")
        endif()
    endif()
endif()

# We need this to find the paths to qdbusxml2cpp and co
if (WITH_DBUS)
    find_package(Qt5DBus REQUIRED)
    include_directories(${Qt5DBus_INCLUDES})
    add_definitions(${Qt5DBus_DEFINITIONS})
endif (WITH_DBUS)
include_directories(${Qt5Core_INCLUDES})
add_definitions(${Qt5Core_DEFINITIONS})
if (NOT WIN32) #implied on Win32
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif(NOT WIN32)
#        set(CMAKE_CXX_FLAGS "${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")

if(APPLE AND NOT TOKEN_AUTH_ONLY)
    include_directories(${Qt5MacExtras_INCLUDE_DIRS})
    add_definitions(${Qt5MacExtras_DEFINITIONS})
    set (QT_LIBRARIES ${QT_LIBRARIES} ${Qt5MacExtras_LIBRARIES})
endif()

if(NOT BUILD_LIBRARIES_ONLY)
    macro(qt_wrap_ui)
	qt5_wrap_ui(${ARGN})
    endmacro()
else()
    # hack
    SET(QT_UIC_EXECUTABLE "") 
endif()

macro(qt_add_resources)
    qt5_add_resources(${ARGN})
endmacro()

if(NOT TOKEN_AUTH_ONLY)
    find_package(Qt5LinguistTools)
    if(Qt5LinguistTools_FOUND) 
        macro(qt_add_translation)
	    qt5_add_translation(${ARGN})
        endmacro()
    else()
         macro(qt_add_translation)
         endmacro()
    endif()
else()
    macro(qt_add_translation)
    endmacro()
endif()

macro(qt_add_dbus_interface)
    qt5_add_dbus_interface(${ARGN})
endmacro()

macro(qt_add_dbus_adaptor)
    qt5_add_dbus_adaptor(${ARGN})
endmacro()

macro(qt_wrap_cpp)
    qt5_wrap_cpp(${ARGN})
endmacro()

macro(install_qt_executable)
    install_qt5_executable(${ARGN})
endmacro()

macro(setup_qt)
endmacro()

set(QT_RCC_EXECUTABLE "${Qt5Core_RCC_EXECUTABLE}")

#Enable deprecated symbols
add_definitions("-DQT_DISABLE_DEPRECATED_BEFORE=0")
add_definitions("-DQT_DEPRECATED_WARNINGS")
add_definitions("-DQT_USE_QSTRINGBUILDER") #optimize string concatenation
add_definitions("-DQT_MESSAGELOGCONTEXT") #enable function name and line number in debug output

