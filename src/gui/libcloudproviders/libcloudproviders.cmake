
# The cloudproviders feature can only be enabled if the libcloudproviders
# and gio-2.0 libraries are available, and failure due to any missing
# dependency should be graceful.
set(LIBCLOUDPROVIDERS_POSSIBLE "")
find_package(Libcloudproviders)
find_package(PkgConfig)
if(LIBCLOUDPROVIDERS_FOUND AND PKG_CONFIG_FOUND)
    pkg_search_module(GIO gio-2.0)
    if(GIO_FOUND)
        set(LIBCLOUDPROVIDERS_POSSIBLE "1")
    endif()
endif()

# User visible config switch
set(WITH_LIBCLOUDPROVIDERS ${LIBCLOUDPROVIDERS_POSSIBLE} CACHE BOOL "Whether to bulid with libcloudproviders")

if(WITH_LIBCLOUDPROVIDERS AND NOT LIBCLOUDPROVIDERS_POSSIBLE)
    message(FATAL_ERROR "Trying to enable libcloudproviders but dependencies are missing")
endif()

if(WITH_LIBCLOUDPROVIDERS)
    target_sources(${APPLICATION_EXECUTABLE} PRIVATE
        libcloudproviders/libcloudproviders.cpp
    )
    target_include_directories(${APPLICATION_EXECUTABLE} SYSTEM PRIVATE ${GIO_INCLUDE_DIRS})
    target_compile_definitions(${APPLICATION_EXECUTABLE} PRIVATE WITH_LIBCLOUDPROVIDERS)
    target_link_libraries(${APPLICATION_EXECUTABLE}
        cloudproviders
        ${GIO_LDFLAGS}
    )

    configure_file(libcloudproviders/cloud-provider.ini.in ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_CLOUDPROVIDERS_DBUS_NAME}.ini)
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_CLOUDPROVIDERS_DBUS_NAME}.ini
        DESTINATION "${DATADIR}/cloud-providers")

    message("Building with libcloudproviders")
elseif(UNIX AND NOT APPLE)
    message("Building without libcloudproviders")
endif()
