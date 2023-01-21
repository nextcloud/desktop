find_package(Qt5 COMPONENTS Core Test Xml Network Qml Quick REQUIRED)

macro(nextcloud_build_test test_class)
    set(CMAKE_AUTOMOC TRUE)
    set(OWNCLOUD_TEST_CLASS ${test_class})
    string(TOLOWER "${OWNCLOUD_TEST_CLASS}" OWNCLOUD_TEST_CLASS_LOWERCASE)

    add_executable(${OWNCLOUD_TEST_CLASS}Test test${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp)
    set_target_properties(${OWNCLOUD_TEST_CLASS}Test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

    target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE
      Nextcloud::sync
      testutils
      nextcloudCore
      cmdCore
      Qt5::Test
      Qt5::Quick
    )

    if (WIN32)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE nextcloudsync_vfs_cfapi)
    elseif (LINUX)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE nextcloudsync_vfs_xattr)
    endif()

    IF(BUILD_UPDATER)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE updater)
    endif()

    add_definitions(-DOWNCLOUD_TEST)
    add_definitions(-DOWNCLOUD_BIN_PATH="${CMAKE_BINARY_DIR}/bin")

    target_include_directories(${OWNCLOUD_TEST_CLASS}Test PRIVATE
        "${CMAKE_SOURCE_DIR}/test/"
        ${CMAKE_SOURCE_DIR}/src/3rdparty/qtokenizer
    )
    set_target_properties(${OWNCLOUD_TEST_CLASS}Test PROPERTIES FOLDER Tests)
endmacro()

macro(nextcloud_add_test test_class)
    set(CMAKE_AUTOMOC TRUE)
    set(OWNCLOUD_TEST_CLASS ${test_class})
    string(TOLOWER "${OWNCLOUD_TEST_CLASS}" OWNCLOUD_TEST_CLASS_LOWERCASE)

    add_executable(${OWNCLOUD_TEST_CLASS}Test test${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp)
    set_target_properties(${OWNCLOUD_TEST_CLASS}Test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

    target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE
      Nextcloud::sync
      testutils
      nextcloudCore
      cmdCore
      Qt5::Test
      Qt5::Quick
    )

    if (WIN32)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE
            nextcloudsync_vfs_cfapi
        )
    endif()

    if (LINUX)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE
            nextcloudsync_vfs_xattr
        )
    endif()

    IF(BUILD_UPDATER)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE
            updater
        )
    endif()

    add_definitions(-DOWNCLOUD_TEST)
    add_definitions(-DOWNCLOUD_BIN_PATH="${CMAKE_BINARY_DIR}/bin")
    add_test(NAME ${OWNCLOUD_TEST_CLASS}Test
        COMMAND ${OWNCLOUD_TEST_CLASS}Test
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

    target_include_directories(${OWNCLOUD_TEST_CLASS}Test
        PRIVATE
        "${CMAKE_SOURCE_DIR}/test/"
        ${CMAKE_SOURCE_DIR}/src/3rdparty/qtokenizer
        )
    set_target_properties(${OWNCLOUD_TEST_CLASS}Test PROPERTIES FOLDER Tests)
endmacro()

macro(nextcloud_add_benchmark test_class)
    set(CMAKE_AUTOMOC TRUE)
    set(OWNCLOUD_TEST_CLASS ${test_class})
    string(TOLOWER "${OWNCLOUD_TEST_CLASS}" OWNCLOUD_TEST_CLASS_LOWERCASE)

    add_executable(${OWNCLOUD_TEST_CLASS}Bench benchmarks/bench${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp)
    set_target_properties(${OWNCLOUD_TEST_CLASS}Bench PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

    target_link_libraries(${OWNCLOUD_TEST_CLASS}Bench
      Nextcloud::sync
      testutils
      nextcloudCore
      cmdCore
      Qt5::Core
      Qt5::Test
      Qt5::Xml
      Qt5::Network
    )

    IF(BUILD_UPDATER)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Bench
            updater
        )
    endif()

    add_definitions(-DOWNCLOUD_TEST)
    add_definitions(-DOWNCLOUD_BIN_PATH="${CMAKE_BINARY_DIR}/bin")
    set_target_properties(${OWNCLOUD_TEST_CLASS}Bench PROPERTIES FOLDER Tests/Benchmarks)
endmacro()
