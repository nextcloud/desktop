add_library(owncloudResources SHARED
    client.qrc
    core_theme.qrc
    loadresources.cpp
    resources.cpp
    qmlresources.cpp
    qmlresources.h
    themewatcher.cpp
    themewatcher.h
)

generate_theme(owncloudResources OWNCLOUD_SIDEBAR_ICONS)

# make them available to the whole project
set(OWNCLOUD_SIDEBAR_ICONS ${OWNCLOUD_SIDEBAR_ICONS} CACHE INTERNAL "Sidebar icons" FORCE)

target_link_libraries(owncloudResources PUBLIC Qt::Core Qt::Gui Qt::Quick)
apply_common_target_settings(owncloudResources)
target_include_directories(owncloudResources PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..> $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>)
target_compile_definitions(owncloudResources PRIVATE APPLICATION_SHORTNAME="${APPLICATION_SHORTNAME}")

set_target_properties(owncloudResources PROPERTIES
    OUTPUT_NAME "ownCloudResources"
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

# Handle Translations, pick all client_* files from trans directory.
file(GLOB client_translations ${CMAKE_SOURCE_DIR}/translations/client_*.ts)
qt_add_lrelease(owncloudResources TS_FILES ${client_translations} QM_FILES_OUTPUT_VARIABLE client_compiled_translations)
add_resources_to_target(
    TARGET owncloudResources
    PREFIX translations
    FILES "${client_compiled_translations}"
)

ecm_add_qml_module(owncloudResources
        URI org.ownCloud.resources
        VERSION 1.0
        NAMESPACE OCC
)

ecm_finalize_qml_module(owncloudResources DESTINATION ${KDE_INSTALL_QMLDIR})

install(TARGETS owncloudResources EXPORT ${APPLICATION_SHORTNAME}Config ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
