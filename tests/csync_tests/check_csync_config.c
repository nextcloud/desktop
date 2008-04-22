#define _GNU_SOURCE /* asprintf */
#include <string.h>

#include "support.h"

#include "csync_config.c"

CSYNC *csync;

const char *testconf = "/tmp/check_csync/csync.conf";

static void setup(void) {
  system("mkdir -p /tmp/check_csync");
  csync_create(&csync, "/tmp/csync1", "/tmp/csync2");
  SAFE_FREE(csync->options.config_dir);
  csync->options.config_dir = c_strdup("/tmp/check_csync/");
}

static void teardown(void) {
  csync_destroy(csync);
  system("rm -rf /tmp/check_csync");
}

START_TEST (check_csync_config_copy_default)
{
  fail_unless(csync_config_copy_default(testconf) == 0, NULL);
}
END_TEST

START_TEST (check_csync_config_load)
{
  fail_unless(csync_config_load(csync, testconf) == 0, NULL);
}
END_TEST

static Suite *csync_suite(void) {
  Suite *s = suite_create("csync_config");

  create_case_fixture(s, "check_csync_config_copy_default", check_csync_config_copy_default, setup, teardown);
  create_case_fixture(s, "check_csync_config_load", check_csync_config_load, setup, teardown);

  return s;
}

int main(void) {
  int nf;

  Suite *s = csync_suite();

  SRunner *sr;
  sr = srunner_create(s);
#if 0
  srunner_set_fork_status(sr, CK_NOFORK);
#endif
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

