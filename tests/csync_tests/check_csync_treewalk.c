#include <string.h>

#include "torture.h"

#define CSYNC_TEST 1
#include "csync_config.c"

CSYNC *csync;
int    file_count;

static void setup_local(void **state) {
  (void) state; /* unused */
  assert_int_equal(system("mkdir -p /tmp/check_csync1"), 0);
  assert_int_equal(system("mkdir -p /tmp/check_csync2"), 0);
  assert_int_equal(system("mkdir -p /tmp/check_csync"), 0);
  assert_int_equal(system("echo \"This is test data\" > /tmp/check_csync1/testfile1.txt"), 0);
  assert_int_equal(system("echo \"This is also test data\" > /tmp/check_csync1/testfile2.txt"), 0);

  assert_int_equal(csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2"), 0);
  assert_int_equal(csync_set_config_dir(csync, "/tmp/check_csync/"), 0);
  assert_int_equal(csync_init(csync), 0);
  
  file_count = 0;
  printf("********** setting up local!\n");
}

static void teardown_local(void **state) {
  (void) state; /* unused */
  assert_int_equal(csync_destroy(csync),0 );
  assert_int_equal(system("rm -rf /tmp/check_csync1"), 0);
  assert_int_equal(system("rm -rf /tmp/check_csync2"), 0);
  assert_int_equal(system("rm -rf /tmp/check_csync"), 0);
  printf("********** tearing down local\n");
}

static void setup_remote(void **state) {
  (void) state; /* unused */
  assert_int_equal(system("mkdir -p /tmp/check_csync1"), 0);
  assert_int_equal(system("mkdir -p /tmp/check_csync2"), 0);
  assert_int_equal(system("echo \"This is test data\" > /tmp/check_csync1/testfile1.txt"), 0);
  assert_int_equal(system("echo \"This is also test data\" > /tmp/check_csync1/testfile2.txt"), 0);

  assert_int_equal(csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2"), 0);
  assert_int_equal(csync_set_config_dir(csync, "/tmp/check_csync/"), 0);
  assert_int_equal(csync_init(csync), 0);
  assert_int_equal(csync_update(csync), 0);
  assert_int_equal(csync_reconcile(csync), 0);
  assert_int_equal(csync_propagate(csync), 0);
  
  file_count = 0;
}

static void teardown_remote(void **state) {
  (void) state; /* unused */
  assert_int_equal(csync_destroy(csync), 0);
  assert_int_equal(system("rm -rf /tmp/check_csync1"), 0);
  assert_int_equal(system("rm -rf /tmp/check_csync2"), 0);
}



static int visitor(TREE_WALK_FILE* file, void *userdata)
{
  if( userdata ) printf("Userdata is set!\n");
  assert_non_null(file);
  printf("Found path: %s\n", file->path);
  file_count++;
  return 0;
}

static void check_csync_treewalk_local(void **state)
{
  (void) state; /* unused */
  assert_int_equal(csync_walk_local_tree(csync, &visitor, 0), 0);
  assert_int_equal(file_count, 0);
  assert_int_equal(csync_update(csync), 0);
  assert_int_equal(csync_walk_local_tree(csync, &visitor, 0), 0);
  assert_int_equal(file_count, 2 );
}

static void check_csync_treewalk_remote(void **state)
{
  (void) state; /* unused */
  assert_non_null(csync->remote.tree);

  assert_int_equal(csync_walk_remote_tree(csync, &visitor, 0), 0);
  assert_int_equal(file_count, 0);
  /* reconcile doesn't update the tree */
  assert_int_equal(csync_update(csync), 0);

  assert_int_equal(csync_walk_remote_tree(csync, &visitor, 0), 0);

  assert_int_equal(file_count, 2);
}

static void check_csync_treewalk_local_with_filter(void **state)
{
  (void) state; /* unused */
  assert_int_equal(csync_walk_local_tree(csync, &visitor, 0), 0);
  assert_int_equal(file_count, 0);
  assert_int_equal(csync_update(csync), 0);

  assert_int_equal(csync_walk_local_tree(csync, &visitor, CSYNC_INSTRUCTION_NEW), 0);
  assert_int_equal(file_count, 2 );

  file_count = 0;
  assert_int_equal(csync_walk_local_tree(csync, &visitor, CSYNC_INSTRUCTION_NEW | CSYNC_INSTRUCTION_REMOVE), 0);
  assert_int_equal(file_count, 2 );

  file_count = 0;
  assert_int_equal(csync_walk_local_tree(csync, &visitor, CSYNC_INSTRUCTION_RENAME), 0);
  assert_int_equal(file_count, 0);

}

int torture_run_tests(void)
{
  const UnitTest tests[] = {
    unit_test_setup_teardown(check_csync_treewalk_local, setup_local, teardown_local ),
    unit_test_setup_teardown(check_csync_treewalk_remote, setup_remote, teardown_remote),
    unit_test_setup_teardown(check_csync_treewalk_local_with_filter, setup_local, teardown_local)
  };

  return run_tests(tests);
}

