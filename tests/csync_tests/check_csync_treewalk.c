#define _GNU_SOURCE /* asprintf */
#include <string.h>

#include "support.h"

#define CSYNC_TEST 1
#include "csync_config.c"

CSYNC *csync;
int    file_count;

static void setup_local(void) {
  fail_if(system("mkdir -p /tmp/check_csync1") < 0, "Setup failed");
  fail_if(system("mkdir -p /tmp/check_csync2") < 0, "Setup failed");
  fail_if(system("mkdir -p /tmp/check_csync") < 0, "Setup failed");
  fail_if(system("echo \"This is test data\" > /tmp/check_csync1/testfile1.txt") < 0, NULL);
  fail_if(system("echo \"This is also test data\" > /tmp/check_csync1/testfile2.txt") < 0, NULL);

  fail_if(csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2") < 0, "Setup failed");
  fail_if(csync_set_config_dir(csync, "/tmp/check_csync/") < 0, "Setup failed");
  fail_if(csync_init(csync) < 0, "Init failed");
  
  file_count = 0;
  printf("********** setting up local!\n");
}

static void teardown_local(void) {
  fail_if(csync_destroy(csync) < 0, "Teardown failed");
  fail_if(system("rm -rf /tmp/check_csync1") < 0, "Teardown failed");
  fail_if(system("rm -rf /tmp/check_csync2") < 0, "Teardown failed");
  fail_if(system("rm -rf /tmp/check_csync") < 0, "Teardown failed"); 
  printf("********** tearing down local\n");
}

static void setup_remote(void) {
  fail_if(system("mkdir -p /tmp/check_csync1") < 0, "Setup failed");
  fail_if(system("mkdir -p /tmp/check_csync2") < 0, "Setup failed");
  fail_if(system("echo \"This is test data\" > /tmp/check_csync1/testfile1.txt") < 0, NULL);
  fail_if(system("echo \"This is also test data\" > /tmp/check_csync1/testfile2.txt") < 0, NULL);

  fail_if(csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2") < 0, "Setup failed");
  fail_if(csync_set_config_dir(csync, "/tmp/check_csync/") < 0, "Setup failed");
  fail_if(csync_init(csync) < 0, "Init failed");
  fail_if(csync_update(csync) < 0, "Update failed");
  fail_if(csync_reconcile(csync) < 0, "Reconcile failed");
  fail_if(csync_propagate(csync) < 0, "Propagate failed");
  
  file_count = 0;
}

static void teardown_remote(void) {
  fail_if(csync_destroy(csync) < 0, "Teardown failed");
  fail_if(system("rm -rf /tmp/check_csync1") < 0, "Teardown failed");
  fail_if(system("rm -rf /tmp/check_csync2") < 0, "Teardown failed");
}



static int visitor(TREE_WALK_FILE* file, void *userdata)
{
    if( userdata ) printf("Userdata is set!\n");
    fail_if(file == NULL, "Invalid TREE_WALK_FILE structure");
    printf("Found path: %s\n", file->path);
    file_count++;
    return 0;
}

START_TEST (check_csync_treewalk_local)
{
    fail_if(csync_walk_local_tree(csync, &visitor, 0) < 0, "Local walk needs update first");
    fail_if(file_count != 0, "Local File count not correct (without update)");
    fail_if(csync_update(csync) < 0, "Update failed");
    fail_if(csync_walk_local_tree(csync, &visitor, 0) < 0, "Local walk");
    fail_if(file_count != 2, "Local File count not correct");
}
END_TEST

START_TEST (check_csync_treewalk_remote)
{
    fail_if(csync->remote.tree == NULL, "Csync remote tree zero" );
    
    fail_if(csync_walk_remote_tree(csync, &visitor, 0) < 0, "Local walk needs update first");
    fail_if(file_count != 0, "Remote File count not correct: %d (without update)", file_count);
    /* reconcile doesn't update the tree */
    fail_if(csync_update(csync) < 0, "Update failed");
    
    fail_if(csync_walk_remote_tree(csync, &visitor, 0) < 0, "Remote walk finds nothing");
    
    fail_if(file_count != 2, "Remote File count not correct: %d", file_count);
}
END_TEST

START_TEST (check_csync_treewalk_local_with_filter)
{
    fail_if(csync_walk_local_tree(csync, &visitor, 0) < 0, "Local walk needs update first");
    fail_if(file_count != 0, "Local File count not correct (without update)");
    fail_if(csync_update(csync) < 0, "Update failed");

    file_count = 0;
    fail_if(csync_walk_local_tree(csync, &visitor, CSYNC_INSTRUCTION_RENAME) < 0, "Local walk");
    fail_if(file_count != 0, "Local File count filtered (RENAME) not correct");

}
END_TEST


static Suite *make_csync_suite(void) {
  Suite *s = suite_create("csync_treewalk");

  create_case_fixture(s, "check_csync_treewalk_local", check_csync_treewalk_local, setup_local, teardown_local);
  create_case_fixture(s, "check_csync_treewalk_remote", check_csync_treewalk_remote, setup_remote, teardown_remote);
  create_case_fixture(s, "check_csync_treewalk_local_with_filter", check_csync_treewalk_local_with_filter, setup_local, teardown_local);

  return s;
}

int main(int argc, char **argv) {
  Suite *s = NULL;
  SRunner *sr = NULL;
  struct argument_s arguments;
  int nf;

  ZERO_STRUCT(arguments);

  cmdline_parse(argc, argv, &arguments);

  s = make_csync_suite();

  sr = srunner_create(s);
  if (arguments.nofork) {
    srunner_set_fork_status(sr, CK_NOFORK);
  }
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

