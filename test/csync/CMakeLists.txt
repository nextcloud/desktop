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
set_target_properties(torture PROPERTIES C_STANDARD 99 AUTOMOC OFF)

set(TEST_TARGET_LIBRARIES torture Qt5::Core "${csync_NAME}")

# create tests

# std
add_cmocka_test(check_std_c_alloc std_tests/check_std_c_alloc.c ${TEST_TARGET_LIBRARIES})

add_cmocka_test(check_std_c_jhash std_tests/check_std_c_jhash.c ${TEST_TARGET_LIBRARIES})

add_cmocka_test(check_std_c_str std_tests/check_std_c_str.c ${TEST_TARGET_LIBRARIES})

# vio
add_cmocka_test(check_vio_ext vio_tests/check_vio_ext.cpp ${TEST_TARGET_LIBRARIES})

if(NOT HAVE_ASPRINTF AND NOT HAVE___MINGW_ASPRINTF)
    target_sources(check_vio_ext PRIVATE ${PROJECT_SOURCE_DIR}/src/csync/std/asprintf.c)
endif()

# encoding
add_cmocka_test(check_encoding_functions encoding_tests/check_encoding.cpp ${TEST_TARGET_LIBRARIES})

