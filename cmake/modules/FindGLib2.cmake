# FindGLib2.cmake

# GLib2_FOUND - System has GLib2
# GLib2_INCLUDES - The GLib2 include directories
# GLib2_LIBRARIES - The libraries needed to use GLib2
# GLib2_DEFINITIONS - Compiler switches required for using GLib2

find_package(PkgConfig)
pkg_check_modules(GLib2 QUIET glib-2.0)
set(GLib2_DEFINITIONS ${GLib2_CFLAGS_OTHER})

find_path(GLib2_INCLUDE_DIR
          NAMES glib.h glib-object.h
          HINTS ${GLib2_INCLUDEDIR} ${GLib2_INCLUDE_DIRS}
          PATH_SUFFIXES glib-2.0)
find_path(GLIBCONFIG_INCLUDE_DIR
          NAMES glibconfig.h
          HINTS ${LIBDIR} ${LIBRARY_DIRS} ${_GLib2_LIBRARY_DIR}
                ${GLib2_INCLUDEDIR} ${GLib2_INCLUDE_DIRS}
                ${CMAKE_EXTRA_INCLUDES} ${CMAKE_EXTRA_LIBRARIES} 
          PATH_SUFFIXES glib-2.0 glib-2.0/include)
list(APPEND GLib2_INCLUDE_DIR ${GLIBCONFIG_INCLUDE_DIR})

find_library(GLib2_LIBRARY
             NAMES glib-2.0 libglib-2.0
             HINTS ${GLib2_LIBDIR} ${GLib2_LIBRARY_DIRS})

set(GLib2_LIBRARIES ${GLib2_LIBRARY})
set(GLib2_INCLUDE_DIRS ${GLib2_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLib2 DEFAULT_MSG
                                  GLib2_LIBRARY GLib2_INCLUDE_DIR)

mark_as_advanced(GLib2_INCLUDE_DIR GLib2_LIBRARY)
