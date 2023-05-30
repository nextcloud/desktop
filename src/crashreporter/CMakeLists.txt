# TODO: differentiate release channel
# if(BUILD_RELEASE)
#     set(CRASHREPORTER_RELEASE_CHANNEL "release")
# else()
     set(CRASHREPORTER_RELEASE_CHANNEL "nightly")
# endif()

# Theme
set(CRASHREPORTER_ICON_DIR "${OEM_THEME_DIR}/theme/colored")

set(CRASHREPORTER_ICON_FILENAME "${APPLICATION_ICON_NAME}-icon.svg")
if (EXISTS "${OEM_THEME_DIR}/theme/colored/${CRASHREPORTER_ICON_FILENAME}")
    set(CRASHREPORTER_ICON ":/${CRASHREPORTER_ICON_FILENAME}")
    set(CRASHREPORTER_ICON_PATH "${CRASHREPORTER_ICON_DIR}/${CRASHREPORTER_ICON_FILENAME}")
else()
    set(CRASHREPORTER_ICON_FILENAME "${APPLICATION_ICON_NAME}-icon.png")
    set(CRASHREPORTER_ICON ":/${CRASHREPORTER_ICON_FILENAME}")
    set(CRASHREPORTER_ICON_SIZE "128")
    set(CRASHREPORTER_ICON_PATH "${CRASHREPORTER_ICON_DIR}/${CRASHREPORTER_ICON_SIZE}-${CRASHREPORTER_ICON_FILENAME}")
    if (NOT EXISTS "${CRASHREPORTER_ICON_PATH}")
        set(CRASHREPORTER_ICON_PATH "${CRASHREPORTER_ICON_DIR}/${APPLICATION_ICON_NAME}-icon-${CRASHREPORTER_ICON_SIZE}.png")
    endif()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc.in
               ${CMAKE_CURRENT_BINARY_DIR}/resources.qrc)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CrashReporterConfig.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/CrashReporterConfig.h)

# Sources
list(APPEND crashreporter_SOURCES main.cpp)
list(APPEND crashreporter_RC "${CMAKE_CURRENT_BINARY_DIR}/resources.qrc")

add_executable(${CRASHREPORTER_EXECUTABLE}
    ${crashreporter_SOURCES}
    ${crashreporter_HEADERS_MOC}
    ${crashreporter_UI_HEADERS}
    ${crashreporter_RC}
)

# This is a GUI Application without its own bundle
set_target_properties(${CRASHREPORTER_EXECUTABLE}
  PROPERTIES
    MACOSX_BUNDLE FALSE
)

target_include_directories(${CRASHREPORTER_EXECUTABLE} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(${CRASHREPORTER_EXECUTABLE}
    PRIVATE
        CrashReporterQt::Gui
        Qt::Core
        Qt::Widgets
)

if(APPLE)
  set_target_properties(${CRASHREPORTER_EXECUTABLE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:owncloud>")
else()
  install(TARGETS ${CRASHREPORTER_EXECUTABLE} ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
endif()
