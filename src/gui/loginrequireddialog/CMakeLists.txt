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
target_compile_definitions(loginrequireddialog
    PRIVATE QT_NO_CAST_TO_ASCII
            QT_NO_CAST_FROM_ASCII
            QT_NO_URL_CAST_FROM_STRING
            QT_NO_CAST_FROM_BYTEARRAY
            QT_USE_QSTRINGBUILDER
            QT_MESSAGELOGCONTEXT  # enable function name and line number in debug output
            QT_DEPRECATED_WARNINGS
            QT_NO_FOREACH
)
