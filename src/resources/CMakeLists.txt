find_package(Qt5LinguistTools)

add_library(owncloudResources SHARED
    client.qrc
    core_theme.qrc
    loadresources.cpp
)

generate_theme(owncloudResources OWNCLOUD_SIDEBAR_ICONS)

# make them available to the whole project
set(OWNCLOUD_SIDEBAR_ICONS ${OWNCLOUD_SIDEBAR_ICONS} CACHE INTERNAL "Sidebar icons" FORCE)

target_link_libraries(owncloudResources PUBLIC Qt5::Core)
apply_common_target_settings(owncloudResources)

set_target_properties(owncloudResources PROPERTIES
    OUTPUT_NAME "${APPLICATION_EXECUTABLE}Resources"
    AUTOUIC ON
    AUTORCC ON
    VERSION ${MIRALL_VERSION}
    SOVERSION ${MIRALL_SOVERSION}
)

generate_export_header(owncloudResources
        EXPORT_MACRO_NAME OWNCLOUDRESOURCES_EXPORT
        EXPORT_FILE_NAME owncloudresources.h
        STATIC_DEFINE OWNCLOUD_BUILT_AS_STATIC
)

if(Qt5LinguistTools_FOUND)
    # Handle Translations, pick all client_* files from trans directory.
    file(GLOB client_translations ${CMAKE_SOURCE_DIR}/translations/client_*.ts)
    qt5_add_translation(client_compiled_translations ${client_translations})
    target_sources(owncloudResources PRIVATE ${client_compiled_translations})
    add_resources_to_target(
        TARGET owncloudResources
        PREFIX translations
        FILES "${client_compiled_translations}"
    )
endif()

install(TARGETS owncloudResources ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
