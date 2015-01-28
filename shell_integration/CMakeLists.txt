add_subdirectory(MacOSX)
add_subdirectory(icons)

if( UNIX AND NOT APPLE )
  add_subdirectory(nautilus)
endif()

