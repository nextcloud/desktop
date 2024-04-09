add_library(spaces
        STATIC
            spacesmodel.cpp
            spacesdelegate.cpp
            spacesbrowser.cpp
            spacesbrowser.ui
            spaceimageprovider.cpp
)
target_link_libraries(spaces PUBLIC Qt::Widgets Qt::QuickWidgets libsync owncloudResources)
set_target_properties(spaces PROPERTIES AUTOUIC ON AUTORCC ON)
apply_common_target_settings(spaces)
