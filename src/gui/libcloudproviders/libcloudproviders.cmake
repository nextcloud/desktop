find_package(PkgConfig REQUIRED)
pkg_check_modules(CLOUDPROVIDERS cloudproviders>=0.3 IMPORTED_TARGET)

if(CLOUDPROVIDERS_FOUND)
        pkg_check_modules(GIO REQUIRED gio-2.0 IMPORTED_TARGET)
        pkg_check_modules(GLIB2 REQUIRED glib-2.0 IMPORTED_TARGET)
endif()

set(LIBCLOUDPROVIDERS_POSSIBLE FALSE)
if (TARGET PkgConfig::CLOUDPROVIDERS)
    set(LIBCLOUDPROVIDERS_POSSIBLE TRUE)
endif()

option(WITH_LIBCLOUDPROVIDERS "Whether to bulid with libcloudproviders" ${LIBCLOUDPROVIDERS_POSSIBLE})

if(WITH_LIBCLOUDPROVIDERS AND NOT LIBCLOUDPROVIDERS_POSSIBLE)
    message(FATAL_ERROR "Trying to enable libcloudproviders but dependencies are missing")
endif()

if(WITH_LIBCLOUDPROVIDERS)
    target_sources(owncloudCore PRIVATE
        libcloudproviders/libcloudproviders.cpp
    )
    target_link_libraries(owncloudCore PUBLIC
            PkgConfig::CLOUDPROVIDERS
            PkgConfig::GLIB2
            PkgConfig::GIO
    )
    target_compile_definitions(owncloudCore PRIVATE WITH_LIBCLOUDPROVIDERS)
    
    configure_file(libcloudproviders/cloud-provider.ini.in ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_CLOUDPROVIDERS_DBUS_NAME}.ini)
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_CLOUDPROVIDERS_DBUS_NAME}.ini
        DESTINATION "${KDE_INSTALL_DATADIR}/cloud-providers")

    message("Building with libcloudproviders")
elseif(UNIX AND NOT APPLE)
    message("Building without libcloudproviders")
endif()

add_feature_info(Libcloudproviders WITH_LIBCLOUDPROVIDERS "Enable cloud provider integration")
