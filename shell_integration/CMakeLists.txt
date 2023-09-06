if (APPLE)
  add_subdirectory(MacOSX)
endif()

if(BUILD_SHELL_INTEGRATION_ICONS)
  add_subdirectory(icons)
endif()

if(MSVC)
  add_subdirectory(windows)
endif()
