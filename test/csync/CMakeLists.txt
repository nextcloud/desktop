include(ECMMarkAsTest)
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}
  ${CMOCKA_INCLUDE_DIR}
)

include_directories(${CHECK_INCLUDE_DIRS})
# create test library
add_library(torture STATIC torture.c cmdline.c)
target_link_libraries(torture ${CMOCKA_LIBRARIES})
set_target_properties(torture PROPERTIES C_STANDARD 99)

set(TEST_TARGET_LIBRARIES torture Qt5::Core "${csync_NAME}")

# create tests

#cmocka does not provide the full path to the test...
unset(CMAKE_RUNTIME_OUTPUT_DIRECTORY)

# std
add_cmocka_test(check_std_c_alloc std_tests/check_std_c_alloc.c ${TEST_TARGET_LIBRARIES})
set_target_properties(check_std_c_alloc PROPERTIES C_STANDARD 99)

add_cmocka_test(check_std_c_jhash std_tests/check_std_c_jhash.c ${TEST_TARGET_LIBRARIES})
set_target_properties(check_std_c_jhash PROPERTIES C_STANDARD 99)

add_cmocka_test(check_std_c_str std_tests/check_std_c_str.c ${TEST_TARGET_LIBRARIES})
set_target_properties(check_std_c_str PROPERTIES C_STANDARD 99)

# vio
if (NOT WIN32)
    # TODO: char* vs wchar_t
    add_cmocka_test(check_vio_ext vio_tests/check_vio_ext.cpp ${TEST_TARGET_LIBRARIES})
    set_target_properties(check_vio_ext PROPERTIES C_STANDARD 99)
endif()

# encoding
add_cmocka_test(check_encoding_functions encoding_tests/check_encoding.cpp ${TEST_TARGET_LIBRARIES})
set_target_properties(check_encoding_functions PROPERTIES C_STANDARD 99)

