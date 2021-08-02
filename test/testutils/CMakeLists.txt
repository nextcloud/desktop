add_library(syncenginetestutils STATIC syncenginetestutils.cpp testutils.cpp)
target_link_libraries(syncenginetestutils PUBLIC owncloudCore Qt5::Test)

# testutilsloader.cpp uses Q_COREAPP_STARTUP_FUNCTION which can't used reliably in a static lib
# therefore we compile it in the tests
if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.12.0")
    add_library(testutilsloader OBJECT testutilsloader.cpp)
    target_link_libraries(testutilsloader PUBLIC owncloudCore)
else()
    add_library(testutilsloader INTERFACE)
    target_sources(testutilsloader INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/testutilsloader.cpp)
endif()
