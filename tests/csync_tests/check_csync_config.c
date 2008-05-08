#define _GNU_SOURCE /* asprintf */
#include <string.h>

#include "support.h"

#define CSYNC_TEST 1
#include "csync_config.c"

CSYNC *csync;

const char *testconf = "/tmp/check_csync1/csync.conf";

static void setup(void) {
  fail_if(system("mkdir -p /tmp/check_csync1") < 0, "Setup failed");
  fail_if(csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2") < 0, "Setup failed");
  SAFE_FREE(csync->options.config_dir);
  csync->options.config_dir = c_strdup("/tmp/check_csync1/");
}

static void teardown(void) {
  fail_if(csync_destroy(csync) < 0, "Teardown failed");
  fail_if(system("rm -rf /tmp/check_csync") < 0, "Teardown failed");
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

static Suite *make_csync_suite(void) {
  Suite *s = suite_create("csync_config");

  create_case_fixture(s, "check_csync_config_copy_default", check_csync_config_copy_default, setup, teardown);
  create_case_fixture(s, "check_csync_config_load", check_csync_config_load, setup, teardown);

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

