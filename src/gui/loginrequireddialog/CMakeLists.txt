add_library(loginrequireddialog STATIC
    loginrequireddialog.cpp
    loginrequireddialog.ui

    abstractloginrequiredwidget.cpp

    oauthloginrequiredwidget.cpp
    oauthloginrequiredwidget.ui

    httploginrequiredwidget.cpp
    httploginrequiredwidget.ui
)
target_link_libraries(loginrequireddialog PUBLIC Qt5::Widgets libsync)
set_target_properties(loginrequireddialog PROPERTIES AUTOUIC ON AUTORCC ON)
target_include_directories(loginrequireddialog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
apply_common_target_settings(loginrequireddialog)

