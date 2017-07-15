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
    set_target_properties(${OWNCLOUD_TEST_CLASS}Test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

    target_link_libraries(${OWNCLOUD_TEST_CLASS}Test
        updater
        ${APPLICATION_EXECUTABLE}sync
        ${QT_QTTEST_LIBRARY}
        ${QT_QTCORE_LIBRARY}
    )

    add_definitions(-DOWNCLOUD_TEST)
    add_definitions(-DOWNCLOUD_BIN_PATH="${CMAKE_BINARY_DIR}/bin")
    add_test(NAME ${OWNCLOUD_TEST_CLASS}Test COMMAND ${OWNCLOUD_TEST_CLASS}Test)
endmacro()

macro(owncloud_add_benchmark test_class additional_cpp)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                        ${QT_INCLUDES}
                        "${PROJECT_SOURCE_DIR}/src/gui"
                        "${PROJECT_SOURCE_DIR}/src/libsync"
                        "${CMAKE_BINARY_DIR}/src/libsync"
                        "${CMAKE_CURRENT_BINARY_DIR}"
                       )

    set(CMAKE_AUTOMOC TRUE)
    set(OWNCLOUD_TEST_CLASS ${test_class})
    string(TOLOWER "${OWNCLOUD_TEST_CLASS}" OWNCLOUD_TEST_CLASS_LOWERCASE)

    add_executable(${OWNCLOUD_TEST_CLASS}Bench benchmarks/bench${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp ${additional_cpp})
    qt5_use_modules(${OWNCLOUD_TEST_CLASS}Bench Test Sql Xml Network)
    set_target_properties(${OWNCLOUD_TEST_CLASS}Bench PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

    target_link_libraries(${OWNCLOUD_TEST_CLASS}Bench
        updater
        ${APPLICATION_EXECUTABLE}sync
        ${QT_QTTEST_LIBRARY}
        ${QT_QTCORE_LIBRARY}
    )

    add_definitions(-DOWNCLOUD_TEST)
    add_definitions(-DOWNCLOUD_BIN_PATH="${CMAKE_BINARY_DIR}/bin")
endmacro()
