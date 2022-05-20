add_library(folderwizard STATIC
    folderwizard.cpp
    folderwizardsourcepage.ui
    folderwizardtargetpage.ui

    spacespage.cpp
    spacespage.ui
)

target_link_libraries(folderwizard PUBLIC Qt5::Widgets libsync)
set_target_properties(folderwizard PROPERTIES AUTOUIC ON AUTORCC ON)
target_compile_definitions(folderwizard
    PRIVATE QT_NO_CAST_TO_ASCII
            QT_NO_CAST_FROM_ASCII
            QT_NO_URL_CAST_FROM_STRING
            QT_NO_CAST_FROM_BYTEARRAY
            QT_USE_QSTRINGBUILDER
            QT_MESSAGELOGCONTEXT  # enable function name and line number in debug output
            QT_DEPRECATED_WARNINGS
            QT_NO_FOREACH
)
