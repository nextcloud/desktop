macro(owncloud_add_test test_class additional_cpp)
    include_directories(${QT_INCLUDES}
                        "${PROJECT_SOURCE_DIR}/src/gui"
                        "${PROJECT_SOURCE_DIR}/src/libsync"
                        "${CMAKE_BINARY_DIR}/src/libsync"
                        "${CMAKE_CURRENT_BINARY_DIR}"
                       )

    set(CMAKE_AUTOMOC TRUE)
    set(OWNCLOUD_TEST_CLASS ${test_class})
    string(TOLOWER "${OWNCLOUD_TEST_CLASS}" OWNCLOUD_TEST_CLASS_LOWERCASE)

    add_executable(${OWNCLOUD_TEST_CLASS}Test test${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp ${additional_cpp})
    qt5_use_modules(${OWNCLOUD_TEST_CLASS}Test Test Sql Xml Network)

    target_link_libraries(${OWNCLOUD_TEST_CLASS}Test
        updater
        ${APPLICATION_EXECUTABLE}sync
        ${QT_QTTEST_LIBRARY}
        ${QT_QTCORE_LIBRARY}
    )

    add_definitions(-DOWNCLOUD_TEST)
    add_definitions(-DOWNCLOUD_BIN_PATH=${CMAKE_BINARY_DIR}/bin)
    add_test(NAME ${OWNCLOUD_TEST_CLASS}Test COMMAND ${OWNCLOUD_TEST_CLASS}Test)
endmacro()
