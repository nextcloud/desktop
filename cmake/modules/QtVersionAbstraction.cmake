include (MacroOptionalFindPackage)
include (MacroLogFeature)


option(BUILD_WITH_QT4 "Build with Qt4 no matter if Qt5 was found" ON)

if( NOT BUILD_WITH_QT4 )
    find_package(Qt5Core QUIET)
    if( Qt5Core_DIR )
        find_package(Qt5Widgets QUIET)
        find_package(Qt5Quick QUIET)
        find_package(Qt5PrintSupport QUIET)
        find_package(Qt5WebKit QUIET)
        find_package(Qt5Location QUIET)
        find_package(Qt5Network QUIET)
        find_package(Qt5Sensors QUIET)
        find_package(Qt5Xml QUIET)
#        find_package(Qt5WebKitWidgets QUIET)

        message(STATUS "Using Qt 5!")

       # We need this to find the paths to qdbusxml2cpp and co
        if (WITH_DBUS)
            find_package(Qt5DBus REQUIRED)
            include_directories(${Qt5DBus_INCLUDES})
            add_definitions(${Qt5DBus_DEFINITIONS})
        endif (WITH_DBUS)

        include_directories(${Qt5Widgets_INCLUDES})
        add_definitions(${Qt5Widgets_DEFINITIONS})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
#        set(CMAKE_CXX_FLAGS "${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")


        macro(qt_wrap_ui)
            qt5_wrap_ui(${ARGN})
        endmacro()

        macro(qt_add_resources)
            qt5_add_resources(${ARGN})
        endmacro()

#        find_package(Qt5LinguistTools REQUIRED)
        macro(qt_add_translation)
#            qt5_add_translation(${ARGN})
        endmacro()

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
    endif()
endif()
if( NOT Qt5Core_DIR )
    message(STATUS "Could not find Qt5, searching for Qt4 instead...")

    set(NEEDED_QT4_COMPONENTS "QtCore" "QtXml" "QtNetwork" "QtGui" "QtWebkit")
    if( BUILD_TESTS )
        list(APPEND NEEDED_QT4_COMPONENTS "QtTest")
    endif()

    macro_optional_find_package(Qt4 4.7.0 COMPONENTS ${NEEDED_QT4_COMPONENTS} )
    macro_log_feature(QT4_FOUND "Qt" "A cross-platform application and UI framework" "http://qt.nokia.com" TRUE "" "If you see this, although libqt4-devel is installed, check whether the \n     qtwebkit-devel package and whatever contains QtUiTools is installed too")

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
endif()

if( Qt5Core_DIR )
    set( HAVE_QT5 TRUE )
else( Qt5Core_DIR )
    set( HAVE_QT5 FALSE )
endif( Qt5Core_DIR )
