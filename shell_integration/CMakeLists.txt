if (APPLE)
  add_subdirectory(MacOSX)
endif()

if(MSVC)
  add_subdirectory(windows)
endif()
