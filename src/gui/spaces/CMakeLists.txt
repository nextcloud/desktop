add_library(ownCloudSpaces SHARED
            spacesmodel.cpp
            spacesbrowser.cpp
            spacesbrowser.ui
            spaceimageprovider.cpp
)
target_link_libraries(ownCloudSpaces PRIVATE Qt::Widgets Qt::QuickWidgets libsync owncloudResources qmlutils)
set_target_properties(ownCloudSpaces PROPERTIES AUTOUIC ON AUTORCC ON)
apply_common_target_settings(ownCloudSpaces)
generate_export_header(ownCloudSpaces
        EXPORT_MACRO_NAME
        SPACES_EXPORT
        EXPORT_FILE_NAME spaceslib.h)

ecm_add_qml_module(ownCloudSpaces
        URI org.ownCloud.gui.spaces
        VERSION 1.0
        NAMESPACE OCC
        QML_FILES
            qml/SpaceDelegate.qml
            qml/SpacesView.qml
)

ecm_finalize_qml_module(ownCloudSpaces DESTINATION ${KDE_INSTALL_QMLDIR})
install(TARGETS ownCloudSpaces ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
