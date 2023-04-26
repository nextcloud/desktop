add_library(loginrequireddialog STATIC
    loginrequireddialog.cpp
    loginrequireddialog.ui

    abstractloginwidget.cpp

    oauthloginwidget.cpp
    oauthloginwidget.ui

    basicloginwidget.cpp
    basicloginwidget.ui
)
target_link_libraries(loginrequireddialog PUBLIC Qt::Widgets libsync owncloudResources)
set_target_properties(loginrequireddialog PROPERTIES AUTOUIC ON AUTORCC ON)
target_include_directories(loginrequireddialog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
apply_common_target_settings(loginrequireddialog)
