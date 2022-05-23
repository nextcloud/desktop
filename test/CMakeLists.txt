include(DefinePlatformDefaults)

include(nextcloud_add_test.cmake)

set(CMAKE_AUTOMOC TRUE)

add_library(testutils
  STATIC
  syncenginetestutils.cpp
  pushnotificationstestutils.cpp
  themeutils.cpp
  testhelper.cpp
)

target_link_libraries(testutils PUBLIC Nextcloud::sync Qt5::Test)
target_include_directories(testutils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(testutils PROPERTIES FOLDER Tests)

if( UNIX AND NOT APPLE )
    nextcloud_add_test(InotifyWatcher)
endif(UNIX AND NOT APPLE)

nextcloud_add_test(LongPath)
nextcloud_add_test(Theme)
nextcloud_add_test(IconUtils)

configure_file(test_journal.db "${PROJECT_BINARY_DIR}/bin/test_journal.db" COPYONLY)

find_package(CMocka)
if (CMOCKA_FOUND)
    include(AddCMockaTest)
    add_subdirectory(csync)
endif (CMOCKA_FOUND)
