add_library(syncenginetestutils STATIC syncenginetestutils.cpp testutils.cpp)
target_link_libraries(syncenginetestutils PUBLIC owncloudCore Qt5::Test)

# testutilsloader.cpp uses Q_COREAPP_STARTUP_FUNCTION which can't used reliably in a static lib
# therefore we compile it in the tests
add_library(testutilsloader OBJECT testutilsloader.cpp)
target_link_libraries(testutilsloader PUBLIC owncloudCore owncloudResources)
