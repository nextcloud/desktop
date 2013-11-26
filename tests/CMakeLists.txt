project(tests C)

set(TORTURE_LIBRARY torture)

include_directories(
  ${CSYNC_PUBLIC_INCLUDE_DIRS}
  ${CSTDLIB_PUBLIC_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}
  ${CMOCKA_INCLUDE_DIR}
  ${HTTPBF_PUBLIC_INCLUDE_DIRS}
)

include_directories(${CHECK_INCLUDE_DIRS})
# create test library
add_library(${TORTURE_LIBRARY} STATIC torture.c cmdline.c)
target_link_libraries(${TORTURE_LIBRARY} ${CMOCKA_LIBRARIES} ${CSYNC_LIBRARY} ${CSTDLIB_LIBRARY})

set(TEST_TARGET_LIBRARIES ${TORTURE_LIBRARY})

# create tests

# std
add_cmocka_test(check_std_c_alloc std_tests/check_std_c_alloc.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_std_c_dir std_tests/check_std_c_dir.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_std_c_file std_tests/check_std_c_file.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_std_c_jhash std_tests/check_std_c_jhash.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_std_c_list std_tests/check_std_c_list.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_std_c_path std_tests/check_std_c_path.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_std_c_rbtree std_tests/check_std_c_rbtree.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_std_c_str std_tests/check_std_c_str.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_std_c_time std_tests/check_std_c_time.c ${TEST_TARGET_LIBRARIES})

# csync tests
# This will be rewritten soon anyway.
#add_cmocka_test(check_logger log_tests/check_log.c ${TEST_TARGET_LIBRARIES})

add_cmocka_test(check_csync_create csync_tests/check_csync_create.c ${TEST_TARGET_LIBRARIES})
# Disable lock testing for us.
# if(NOT WIN32)
# add_cmocka_test(check_csync_lock csync_tests/check_csync_lock.c ${TEST_TARGET_LIBRARIES})
# endif()
add_cmocka_test(check_csync_log csync_tests/check_csync_log.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_config csync_tests/check_csync_config.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_exclude csync_tests/check_csync_exclude.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_statedb_load csync_tests/check_csync_statedb_load.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_time csync_tests/check_csync_time.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_util csync_tests/check_csync_util.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_misc csync_tests/check_csync_misc.c ${TEST_TARGET_LIBRARIES})

# csync tests which require init
add_cmocka_test(check_csync_init csync_tests/check_csync_init.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_statedb_query csync_tests/check_csync_statedb_query.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_csync_commit csync_tests/check_csync_commit.c ${TEST_TARGET_LIBRARIES})

# treewalk
add_cmocka_test(check_csync_treewalk csync_tests/check_csync_treewalk.c ${TEST_TARGET_LIBRARIES})

# vio
add_cmocka_test(check_vio_handle vio_tests/check_vio_handle.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_vio_file_stat vio_tests/check_vio_file_stat.c ${TEST_TARGET_LIBRARIES})
add_cmocka_test(check_vio vio_tests/check_vio.c ${TEST_TARGET_LIBRARIES})

# sync
add_cmocka_test(check_csync_update csync_tests/check_csync_update.c ${TEST_TARGET_LIBRARIES})

# encoding
add_cmocka_test(check_encoding_functions encoding_tests/check_encoding.c ${TEST_TARGET_LIBRARIES})

# httpbf
set(TEST_HTTPBF_LIBRARIES ${TEST_TARGET_LIBRARIES} ${NEON_LIBRARIES})
add_cmocka_test(check_httpbf httpbf_tests/hbf_send_test.c ${TEST_HTTPBF_LIBRARIES} )


