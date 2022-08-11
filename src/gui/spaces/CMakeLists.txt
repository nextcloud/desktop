add_library(spaces STATIC
	spacesmodel.cpp spacesbrowser.cpp spacesbrowser.ui)
target_link_libraries(spaces PUBLIC Qt5::Widgets libsync)
set_target_properties(spaces PROPERTIES AUTOUIC ON AUTORCC ON)
apply_common_target_settings(spaces)
