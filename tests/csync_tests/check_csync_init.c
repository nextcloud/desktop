#include <string.h>

#include "support.h"

#include "csync_private.h"

CSYNC *csync;

static void setup(void) {
  fail_if(system("mkdir -p /tmp/check_csync1") < 0, "Setup failed");
  fail_if(system("mkdir -p /tmp/check_csync2") < 0, "Setup failed");
  fail_if(csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2") < 0, "Setup failed");
  fail_if(csync_set_config_dir(csync, "/tmp/check_csync") < 0, "Setup failed");
}

static void setup_module(void) {
  fail_if(system("mkdir -p /tmp/check_csync1") < 0, "Setup failed");
  fail_if(system("mkdir -p /tmp/check_csync2") < 0, "Setup failed");
  fail_if(csync_create(&csync, "/tmp/check_csync1", "dummy://foo/bar") < 0, "Setup failed");
  fail_if(csync_set_config_dir(csync, "/tmp/check_csync") < 0, "Setup failed");
}

static void teardown(void) {
  fail_if(csync_destroy(csync) < 0, "Teardown failed");
  fail_if(system("rm -rf /tmp/check_csync") < 0, "Teardown failed");
  fail_if(system("rm -rf /tmp/check_csync1") < 0, "Teardown failed");
  fail_if(system("rm -rf /tmp/check_csync2") < 0, "Teardown failed");
}

START_TEST (check_csync_init_null)
{
  fail_unless(csync_init(NULL) < 0, NULL);
}
END_TEST

START_TEST (check_csync_init)
{
  fail_unless(csync_init(csync) == 0, NULL);

  fail_unless((csync->status & CSYNC_STATUS_INIT) == 1, NULL);

  fail_unless(csync_init(csync) == 1, NULL);
}
END_TEST

START_TEST (check_csync_init_module)
{
  fail_unless(csync_init(csync) == 0, NULL);

  fail_unless((csync->status & CSYNC_STATUS_INIT) == 1, NULL);

  fail_unless(csync_init(csync) == 1, NULL);
}
END_TEST

static Suite *make_csync_suite(void) {
  Suite *s = suite_create("csync");

  create_case_fixture(s, "check_csync_init_null", check_csync_init_null, setup, teardown);
  create_case_fixture(s, "check_csync_init", check_csync_init, setup, teardown);
  create_case_fixture(s, "check_csync_init_module", check_csync_init_module, setup_module, teardown);

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

