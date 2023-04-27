add_executable(cmd
    cmd.cpp
    httpcredentialstext.cpp
)
set_target_properties(cmd PROPERTIES OUTPUT_NAME "${APPLICATION_EXECUTABLE}cmd")
ecm_mark_nongui_executable(cmd)

target_link_libraries(cmd csync libsync Qt5::Core Qt5::Network)
apply_common_target_settings(cmd)

if(APPLE)
  install(TARGETS cmd RUNTIME DESTINATION "${KDE_INSTALL_BUNDLEDIR}/${APPLICATION_EXECUTABLE}.app/Contents/MacOS/")
else()
  install(TARGETS cmd ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
endif()
