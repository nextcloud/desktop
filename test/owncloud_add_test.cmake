macro(owncloud_add_test test_class additional_cpp)
    include_directories(${QT_INCLUDES}
                        "${PROJECT_SOURCE_DIR}/src/gui"
                        "${PROJECT_SOURCE_DIR}/src/libsync"
                        "${CMAKE_BINARY_DIR}/src/libsync"
                        "${CMAKE_CURRENT_BINARY_DIR}"
                       )

    set(OWNCLOUD_TEST_CLASS ${test_class})
    set(CMAKE_AUTOMOC TRUE)
    string(TOLOWER "${OWNCLOUD_TEST_CLASS}" OWNCLOUD_TEST_CLASS_LOWERCASE)
    configure_file(main.cpp.in test${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp)
    configure_file(test${OWNCLOUD_TEST_CLASS_LOWERCASE}.h test${OWNCLOUD_TEST_CLASS_LOWERCASE}.h)
    qt_wrap_cpp(test${OWNCLOUD_TEST_CLASS_LOWERCASE}.h)

    add_executable(${OWNCLOUD_TEST_CLASS}Test test${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp ${additional_cpp})
    qt5_use_modules(${OWNCLOUD_TEST_CLASS}Test Test Sql Xml Network)

    target_link_libraries(${OWNCLOUD_TEST_CLASS}Test
        updater
        ${APPLICATION_EXECUTABLE}sync
        ${QT_QTTEST_LIBRARY}
        ${QT_QTCORE_LIBRARY}
    )

    add_test(NAME ${OWNCLOUD_TEST_CLASS}Test COMMAND ${OWNCLOUD_TEST_CLASS}Test)
endmacro()
