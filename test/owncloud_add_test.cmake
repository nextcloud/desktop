macro(owncloud_add_test test_class)
    include_directories(${QT_INCLUDES} "${PROJECT_SOURCE_DIR}/src" ${CMAKE_CURRENT_BINARY_DIR})

    set(OWNCLOUD_TEST_CLASS ${test_class})
    string(TOLOWER "${OWNCLOUD_TEST_CLASS}" OWNCLOUD_TEST_CLASS_LOWERCASE)
    configure_file(main.cpp.in test${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp)
    configure_file(test${OWNCLOUD_TEST_CLASS_LOWERCASE}.h test${OWNCLOUD_TEST_CLASS_LOWERCASE}.h)
    qt_wrap_cpp(${OWNCLOUD_TEST_CLASS}_MOCS test${OWNCLOUD_TEST_CLASS_LOWERCASE}.h)

    add_executable(${OWNCLOUD_TEST_CLASS}Test test${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp ${${OWNCLOUD_TEST_CLASS}_MOCS})
    qt5_use_modules(${OWNCLOUD_TEST_CLASS}Test Test Sql)

    target_link_libraries(${OWNCLOUD_TEST_CLASS}Test
        ${APPLICATION_EXECUTABLE}sync
        ${QT_QTTEST_LIBRARY}
        ${QT_QTCORE_LIBRARY}
    )

    add_test(NAME ${OWNCLOUD_TEST_CLASS}Test COMMAND ${OWNCLOUD_TEST_CLASS}Test)
endmacro()
