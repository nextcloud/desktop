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
set(CRASHREPORTER_ICON_PATH "${CRASHREPORTER_ICON_DIR}/${CRASHREPORTER_ICON_SIZE}-${CRASHREPORTER_ICON_FILENAME}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc.in
               ${CMAKE_CURRENT_BINARY_DIR}/resources.qrc)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CrashReporterConfig.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/CrashReporterConfig.h)

# Sources
list(APPEND crashreporter_SOURCES main.cpp)
list(APPEND crashreporter_RC "${CMAKE_CURRENT_BINARY_DIR}/resources.qrc")


if(NOT BUILD_LIBRARIES_ONLY)
    add_executable( ${CRASHREPORTER_EXECUTABLE} WIN32
        ${crashreporter_SOURCES}
        ${crashreporter_HEADERS_MOC}
        ${crashreporter_UI_HEADERS}
        ${crashreporter_RC}
    )

    find_package(Qt5 REQUIRED COMPONENTS Widgets)

    target_include_directories(${CRASHREPORTER_EXECUTABLE} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    set_target_properties(${CRASHREPORTER_EXECUTABLE} PROPERTIES AUTOMOC ON)
    set_target_properties(${CRASHREPORTER_EXECUTABLE} PROPERTIES AUTORCC ON)
    set_target_properties(${CRASHREPORTER_EXECUTABLE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY} )
    target_link_libraries(${CRASHREPORTER_EXECUTABLE}
        crashreporter-gui
        Qt5::Core Qt5::Widgets
    )

    if(BUILD_OWNCLOUD_OSX_BUNDLE)
        install(TARGETS ${CRASHREPORTER_EXECUTABLE} DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS)
    elseif(NOT BUILD_LIBRARIES_ONLY)
        install(TARGETS ${CRASHREPORTER_EXECUTABLE}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
endif()
