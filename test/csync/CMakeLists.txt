include(ECMMarkAsTest)

# create test library
add_library(torture STATIC torture.c)
target_link_libraries(torture ${CMOCKA_LIBRARIES})
set_target_properties(torture PROPERTIES C_STANDARD 99 AUTOMOC OFF)
target_include_directories(torture
    PUBLIC ${CMOCKA_INCLUDE_DIR}
)

set(TEST_TARGET_LIBRARIES torture Qt5::Core csync)

# create tests

# std

add_cmocka_test(check_std_c_jhash std_tests/check_std_c_jhash.c ${TEST_TARGET_LIBRARIES})
