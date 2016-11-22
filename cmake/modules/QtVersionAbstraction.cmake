# (c) 2014 Copyright ownCloud GmbH
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING* file.

include (MacroOptionalFindPackage)
include (MacroLogFeature)

option(BUILD_WITH_QT4 "Build with Qt4 no matter if Qt5 was found" OFF)

if( BUILD_WITH_QT4 )
    message(STATUS "Search for Qt5 was disabled by option BUILD_WITH_QT4")
else( BUILD_WITH_QT4 )
    find_package(Qt5Core QUIET)
endif( BUILD_WITH_QT4 )

if( Qt5Core_FOUND )
    message(STATUS "Found Qt5 core, checking for further dependencies...")
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

else( Qt5Core_FOUND )
    if(WIN32 OR APPLE)
    if (NOT BUILD_WITH_QT4)
	message(FATAL_ERROR "Qt 5 not found, but application depends on Qt5 on Windows and Mac OS X")
    endif ()
    endif(WIN32 OR APPLE)
endif( Qt5Core_FOUND )


if( Qt5Core_FOUND )
    message(STATUS "Using Qt 5!")

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

    add_definitions("-DQT_USE_QSTRINGBUILDER") #optimize string concatenation
    add_definitions("-DQT_MESSAGELOGCONTEXT") #enable function name and line number in debug output
endif( Qt5Core_FOUND )

if(NOT Qt5Core_FOUND)
    message(STATUS "Could not find Qt5, searching for Qt4 instead...")

    set(NEEDED_QT4_COMPONENTS "QtCore" "QtXml" "QtNetwork" "QtGui" "QtWebkit")
    if( BUILD_TESTS )
        list(APPEND NEEDED_QT4_COMPONENTS "QtTest")
    endif()

    find_package(Qt4 4.7.0 COMPONENTS ${NEEDED_QT4_COMPONENTS} )
    macro_log_feature(QT4_FOUND "Qt" "A cross-platform application and UI framework" "http://www.qt-project.org" TRUE "" "If you see this, although libqt4-devel is installed, check whether the \n     qtwebkit-devel package and whatever contains QtUiTools is installed too")

    macro(qt5_use_modules)
    endmacro()

    macro(qt_wrap_ui)
        qt4_wrap_ui(${ARGN})
    endmacro()

    macro(qt_add_resources)
        qt4_add_resources(${ARGN})
    endmacro()

    macro(qt_add_translation)
        qt4_add_translation(${ARGN})
    endmacro()

    macro(qt_add_dbus_interface)
      qt4_add_dbus_interface(${ARGN})
    endmacro()

    macro(qt_add_dbus_adaptor)
        qt4_add_dbus_adaptor(${ARGN})
    endmacro()

    macro(qt_wrap_cpp)
        qt4_wrap_cpp(${ARGN})
    endmacro()

    macro(install_qt_executable)
        install_qt4_executable(${ARGN})
    endmacro()

    macro(setup_qt)
        set(QT_USE_QTGUI TRUE)
        set(QT_USE_QTSQL TRUE)
        set(QT_USE_QTNETWORK TRUE)
        set(QT_USE_QTXML TRUE)
        set(QT_USE_QTWEBKIT TRUE)
        set(QT_USE_QTDBUS TRUE)

        include( ${QT_USE_FILE} )
    endmacro()

    if (CMAKE_COMPILER_IS_GNUCC)
        execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
                OUTPUT_VARIABLE GCC_VERSION)
        if (GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL 4.7)
            add_definitions("-DQ_DECL_OVERRIDE=override")
        else()
            add_definitions("-DQ_DECL_OVERRIDE=")
        endif()
    else() #clang or others
        add_definitions("-DQ_DECL_OVERRIDE=override")
    endif()

endif()

if( Qt5Core_DIR )
    set( HAVE_QT5 TRUE )
else( Qt5Core_DIR )
    set( HAVE_QT5 FALSE )
endif( Qt5Core_DIR )
