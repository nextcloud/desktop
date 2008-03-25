project(tests)

set(SUPPORT_LIBRARY support)

include_directories(
  ${CSYNC_PUBLIC_INCLUDE_DIRS}
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${CHECK_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}
)

# create test library
add_library(${SUPPORT_LIBRARY} STATIC support.c)
target_link_libraries(${SUPPORT_LIBRARY} ${CHECK_LIBRARIES} ${CSYNC_LIBRARY} ${CSTDLIB_LIBRARY})

set(TEST_TARGET_LIBRARIES ${SUPPORT_LIBRARY})

add_definitions(-DSOURCEDIR='\"${CMAKE_SOURCE_DIR}\"')

# create tests

# std
macro_add_check_test(check_std_c_alloc std_tests/check_std_c_alloc.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_dir std_tests/check_std_c_dir.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_file std_tests/check_std_c_file.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_jhash std_tests/check_std_c_jhash.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_path std_tests/check_std_c_path.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_rbtree std_tests/check_std_c_rbtree.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_std_c_str std_tests/check_std_c_str.c ${TEST_TARGET_LIBRARIES})

# csync
macro_add_check_test(check_logger log_tests/check_log.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_csync csync_tests/check_csync.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_csync_lock csync_tests/check_csync_lock.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_csync_config csync_tests/check_csync_config.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_csync_exclude csync_tests/check_csync_exclude.c ${TEST_TARGET_LIBRARIES})
macro_add_check_test(check_csync_journal csync_tests/check_csync_journal.c ${TEST_TARGET_LIBRARIES})

