find_package(Qt5 COMPONENTS Core Test Xml Network REQUIRED)

include(ECMAddTests)

function(owncloud_add_test test_class additional_cpp)
    set(OWNCLOUD_TEST_CLASS ${test_class})
    string(TOLOWER "${OWNCLOUD_TEST_CLASS}" OWNCLOUD_TEST_CLASS_LOWERCASE)
    list(APPEND additional_cpp test${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp)

    ecm_add_test("${additional_cpp}"
        TEST_NAME "${OWNCLOUD_TEST_CLASS}Test"
        LINK_LIBRARIES
        updater
        ${APPLICATION_EXECUTABLE}sync
        Qt5::Core Qt5::Test Qt5::Xml Qt5::Network
    )

    target_compile_definitions(${OWNCLOUD_TEST_CLASS}Test PRIVATE OWNCLOUD_TEST OWNCLOUD_BIN_PATH="${CMAKE_BINARY_DIR}/bin")

    target_include_directories(${OWNCLOUD_TEST_CLASS}Test PRIVATE "${CMAKE_SOURCE_DIR}/test/")
endfunction()

macro(owncloud_add_benchmark test_class additional_cpp)
    set(OWNCLOUD_TEST_CLASS ${test_class})
    string(TOLOWER "${OWNCLOUD_TEST_CLASS}" OWNCLOUD_TEST_CLASS_LOWERCASE)

    add_executable(${OWNCLOUD_TEST_CLASS}Bench benchmarks/bench${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp ${additional_cpp})
    ecm_mark_nongui_executable(${OWNCLOUD_TEST_CLASS}Bench)

    target_link_libraries(${OWNCLOUD_TEST_CLASS}Bench
        updater
        ${APPLICATION_EXECUTABLE}sync
        Qt5::Core Qt5::Test Qt5::Xml Qt5::Network
    )
    target_compile_definitions(${OWNCLOUD_TEST_CLASS}Bench PRIVATE OWNCLOUD_TEST OWNCLOUD_BIN_PATH="${CMAKE_BINARY_DIR}/bin")
endmacro()
