project(tests C)

set(TORTURE_LIBRARY torture)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CSYNC_PUBLIC_INCLUDE_DIRS}
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}
  ${CMOCKA_INCLUDE_DIR}
)

include_directories(${CHECK_INCLUDE_DIRS})
# create test library
add_library(${TORTURE_LIBRARY} STATIC torture.c cmdline.c)
target_link_libraries(${TORTURE_LIBRARY} ${CMOCKA_LIBRARIES} ${CSYNC_LIBRARY} ${CSTDLIB_LIBRARY})

set(TEST_TARGET_LIBRARIES ${TORTURE_LIBRARY})

# create tests

# std
add_cmocka_test(check_std_c_alloc std_tests/check_std_c_alloc.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_std_c_jhash std_tests/check_std_c_jhash.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_std_c_path std_tests/check_std_c_path.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_std_c_rbtree std_tests/check_std_c_rbtree.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_std_c_str std_tests/check_std_c_str.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_std_c_time std_tests/check_std_c_time.c ${TEST_TARGET_LIBRARIES})

# csync tests
# This will be rewritten soon anyway.
#add_cmocka_test(check_logger log_tests/check_log.cpp ${TEST_TARGET_LIBRARIES})

add_cmocka_test(check_csync_create csync_tests/check_csync_create.cpp ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_log csync_tests/check_csync_log.cpp ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_exclude csync_tests/check_csync_exclude.cpp ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_statedb_load csync_tests/check_csync_statedb_load.cpp ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_util csync_tests/check_csync_util.cpp ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_misc csync_tests/check_csync_misc.cpp ${TEST_TARGET_LIBRARIES})

# csync tests which require init
add_cmocka_test(check_csync_init csync_tests/check_csync_init.cpp ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_statedb_query csync_tests/check_csync_statedb_query.cpp ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_commit csync_tests/check_csync_commit.cpp ${TEST_TARGET_LIBRARIES})

# vio
add_cmocka_test(check_vio vio_tests/check_vio.cpp ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_vio_ext vio_tests/check_vio_ext.cpp ${TEST_TARGET_LIBRARIES})

# sync
add_cmocka_test(check_csync_update csync_tests/check_csync_update.cpp ${TEST_TARGET_LIBRARIES})

# encoding
add_cmocka_test(check_encoding_functions encoding_tests/check_encoding.cpp ${TEST_TARGET_LIBRARIES})

