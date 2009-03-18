project(tests C)

set(SUPPORT_LIBRARY support)

include_directories(
  ${CSYNC_PUBLIC_INCLUDE_DIRS}
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${CHECK_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}
)

# create test library
add_library(${SUPPORT_LIBRARY} STATIC support.c cmdline.c)
target_link_libraries(${SUPPORT_LIBRARY} ${CHECK_LIBRARIES} ${CSYNC_LIBRARY} ${CSTDLIB_LIBRARY})

set(TEST_TARGET_LIBRARIES ${SUPPORT_LIBRARY})

# create tests

# std
macro_add_check_test(check_std_c_alloc std_tests/check_std_c_alloc.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_dir std_tests/check_std_c_dir.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_file std_tests/check_std_c_file.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_jhash std_tests/check_std_c_jhash.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_list std_tests/check_std_c_list.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_path std_tests/check_std_c_path.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_rbtree std_tests/check_std_c_rbtree.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_str std_tests/check_std_c_str.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_time std_tests/check_std_c_time.c ${TEST_TARGET_LIBRARIES})

# csync tests
macro_add_check_test(check_logger log_tests/check_log.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_csync_create csync_tests/check_csync_create.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_csync_lock csync_tests/check_csync_lock.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_csync_config csync_tests/check_csync_config.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_csync_exclude csync_tests/check_csync_exclude.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_csync_statedb_load csync_tests/check_csync_statedb_load.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_csync_time csync_tests/check_csync_time.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_csync_util csync_tests/check_csync_util.c ${TEST_TARGET_LIBRARIES})

# csync tests which require init
macro_add_check_test(check_csync_init csync_tests/check_csync_init.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_csync_statedb_query csync_tests/check_csync_statedb_query.c ${TEST_TARGET_LIBRARIES})

# vio
macro_add_check_test(check_vio_handle vio_tests/check_vio_handle.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_vio_file_stat vio_tests/check_vio_file_stat.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_vio_local vio_tests/check_vio_local.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_vio vio_tests/check_vio.c ${TEST_TARGET_LIBRARIES})

# sync
macro_add_check_test(check_csync_update csync_tests/check_csync_update.c ${TEST_TARGET_LIBRARIES})

