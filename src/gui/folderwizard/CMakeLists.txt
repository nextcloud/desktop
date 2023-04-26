add_library(folderwizard STATIC
    folderwizard.cpp
    folderwizardlocalpath.cpp
    folderwizardsourcepage.ui

    folderwizardremotepath.cpp
    folderwizardtargetpage.ui

    folderwizardselectivesync.cpp

    spacespage.cpp
    spacespage.ui
)

target_link_libraries(folderwizard PUBLIC Qt::Widgets libsync)
set_target_properties(folderwizard PROPERTIES AUTOUIC ON AUTORCC ON)
apply_common_target_settings(folderwizard)
