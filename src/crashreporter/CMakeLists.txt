PROJECT( CrashReporter )
cmake_policy(SET CMP0017 NEW)

list(APPEND crashreporter_SOURCES main.cpp)
list(APPEND crashreporter_RC resources.qrc)

qt_wrap_ui( crashreporter_UI_HEADERS ${crashreporter_UI} )
qt_add_resources( crashreporter_RC_RCC ${crashreporter_RC} )


# TODO: differentiate release channel
# if(BUILD_RELEASE)
#     set(CRASHREPORTER_RELEASE_CHANNEL "release")
# else()
     set(CRASHREPORTER_RELEASE_CHANNEL "nightly")
# endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CrashReporterConfig.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/CrashReporterConfig.h)


include_directories(${CMAKE_CURRENT_BINARY_DIR}
    "../3rdparty/libcrashreporter-qt/src/"
)

add_executable( ${CRASHREPORTER_EXECUTABLE} WIN32
    ${crashreporter_SOURCES}
    ${crashreporter_HEADERS_MOC}
    ${crashreporter_UI_HEADERS}
    ${crashreporter_RC_RCC}
)


target_link_libraries( ${CRASHREPORTER_EXECUTABLE}
    crashreporter-gui
    ${QT_LIBRARIES}
)

#TODO: don't use automoc :-(
set_target_properties(${CRASHREPORTER_EXECUTABLE} PROPERTIES AUTOMOC ON)
set_target_properties(${CRASHREPORTER_EXECUTABLE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY} )
install(TARGETS ${CRASHREPORTER_EXECUTABLE} RUNTIME DESTINATION ${LIBEXEC_INSTALL_DIR})

qt5_use_modules(${CRASHREPORTER_EXECUTABLE} Widgets Network)
