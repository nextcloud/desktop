add_library(newwizard STATIC
    enums.cpp

    pages/abstractsetupwizardpage.h
    pages/abstractsetupwizardpage.cpp

    pages/serverurlsetupwizardpage.ui
    pages/serverurlsetupwizardpage.h
    pages/serverurlsetupwizardpage.cpp

    pages/webfingersetupwizardpage.ui
    pages/webfingersetupwizardpage.h
    pages/webfingersetupwizardpage.cpp

    pages/basiccredentialssetupwizardpage.ui
    pages/basiccredentialssetupwizardpage.h
    pages/basiccredentialssetupwizardpage.cpp

    pages/accountconfiguredwizardpage.ui
    pages/accountconfiguredwizardpage.h
    pages/accountconfiguredwizardpage.cpp

    pages/oauthcredentialssetupwizardpage.ui
    pages/oauthcredentialssetupwizardpage.h
    pages/oauthcredentialssetupwizardpage.cpp

    setupwizardwindow.ui
    setupwizardwindow.h
    setupwizardwindow.cpp

    setupwizardcontroller.h
    setupwizardcontroller.cpp

    setupwizardaccountbuilder.cpp
    setupwizardaccountbuilder.h

    navigation.cpp
    navigation.h

    jobs/resolveurljobfactory.cpp
    jobs/resolveurljobfactory.h

    jobs/checkbasicauthjobfactory.cpp
    jobs/checkbasicauthjobfactory.h

    postfixlineedit.cpp

    states/abstractsetupwizardstate.cpp
    states/serverurlsetupwizardstate.cpp
    states/webfingersetupwizardstate.cpp
    states/basiccredentialssetupwizardstate.cpp
    states/oauthcredentialssetupwizardstate.cpp
    states/accountconfiguredsetupwizardstate.cpp

    setupwizardcontext.cpp
)
target_link_libraries(newwizard PUBLIC Qt5::Widgets libsync)
set_target_properties(newwizard PROPERTIES AUTOUIC ON AUTORCC ON)
target_include_directories(newwizard PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(newwizard
    PRIVATE QT_NO_CAST_TO_ASCII
            QT_NO_CAST_FROM_ASCII
            QT_NO_URL_CAST_FROM_STRING
            QT_NO_CAST_FROM_BYTEARRAY
            QT_USE_QSTRINGBUILDER
            QT_MESSAGELOGCONTEXT  # enable function name and line number in debug output
            QT_DEPRECATED_WARNINGS
            QT_NO_FOREACH
)
