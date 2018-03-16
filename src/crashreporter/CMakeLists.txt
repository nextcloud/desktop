PROJECT( CrashReporter )
cmake_policy(SET CMP0017 NEW)

# TODO: differentiate release channel
# if(BUILD_RELEASE)
#     set(CRASHREPORTER_RELEASE_CHANNEL "release")
# else()
     set(CRASHREPORTER_RELEASE_CHANNEL "nightly")
# endif()

# Theme
if(DEFINED OEM_THEME_DIR AND EXISTS "${OEM_THEME_DIR}/theme/colored")
    set(CRASHREPORTER_ICON_DIR "${OEM_THEME_DIR}/theme/colored")
else()
    set(CRASHREPORTER_ICON_DIR "${CMAKE_SOURCE_DIR}/theme/colored")
endif()

set(CRASHREPORTER_ICON_FILENAME "${APPLICATION_ICON_NAME}-icon.png")
set(CRASHREPORTER_ICON ":/${CRASHREPORTER_ICON_FILENAME}")
set(CRASHREPORTER_ICON_SIZE "128")
set(CRASHREPORTER_ICON_PATH "${CRASHREPORTER_ICON_DIR}/${APPLICATION_ICON_NAME}-icon-${CRASHREPORTER_ICON_SIZE}.png")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc.in
               ${CMAKE_CURRENT_BINARY_DIR}/resources.qrc)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CrashReporterConfig.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/CrashReporterConfig.h)

# Sources
list(APPEND crashreporter_SOURCES main.cpp)
list(APPEND crashreporter_RC "${CMAKE_CURRENT_BINARY_DIR}/resources.qrc")

qt_wrap_ui( crashreporter_UI_HEADERS ${crashreporter_UI} )
qt_add_resources( crashreporter_RC_RCC ${crashreporter_RC} )

include_directories(${CMAKE_CURRENT_BINARY_DIR}
    "../3rdparty/libcrashreporter-qt/src/"
)


if(NOT BUILD_LIBRARIES_ONLY)
    add_executable( ${CRASHREPORTER_EXECUTABLE} WIN32
        ${crashreporter_SOURCES}
        ${crashreporter_HEADERS_MOC}
        ${crashreporter_UI_HEADERS}
        ${crashreporter_RC_RCC}
    )

    qt5_use_modules(${CRASHREPORTER_EXECUTABLE} Widgets Network)

    set_target_properties(${CRASHREPORTER_EXECUTABLE} PROPERTIES AUTOMOC ON)
    set_target_properties(${CRASHREPORTER_EXECUTABLE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY} )
    set_target_properties(${CRASHREPORTER_EXECUTABLE} PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}/${APPLICATION_EXECUTABLE}" )
    target_link_libraries(${CRASHREPORTER_EXECUTABLE}
        crashreporter-gui
        ${QT_LIBRARIES}
    )

    if(BUILD_OWNCLOUD_OSX_BUNDLE)
        install(TARGETS ${CRASHREPORTER_EXECUTABLE} DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS)
    elseif(NOT BUILD_LIBRARIES_ONLY)
        install(TARGETS ${CRASHREPORTER_EXECUTABLE}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()

    # FIXME: The following lines are dup in src/gui and src/cmd because it needs to be done after both are installed
    #FIXME: find a nice solution to make the second if(BUILD_OWNCLOUD_OSX_BUNDLE) unnecessary
    # currently it needs to be done because the code right above needs to be executed no matter
    # if building a bundle or not and the install_qt4_executable needs to be called afterwards
    if(BUILD_OWNCLOUD_OSX_BUNDLE)
       get_target_property (QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
       install(CODE "
        message(STATUS \"Deploying (Qt) dependencies and fixing library paths...\")
        execute_process(COMMAND \"${CMAKE_SOURCE_DIR}/admin/osx/macdeployqt.py\" ${CMAKE_INSTALL_PREFIX}/${OWNCLOUD_OSX_BUNDLE} ${QT_QMAKE_EXECUTABLE})
        " COMPONENT RUNTIME)
    endif()
endif()
