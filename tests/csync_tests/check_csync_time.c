#include "support.h"

#include "csync_time.h"

CSYNC *csync;

static void setup(void) {
  fail_if(system("mkdir -p /tmp/check_csync1") < 0, "Setup failed");
  fail_if(system("mkdir -p /tmp/check_csync2") < 0, "Setup failed");
  fail_if(csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2") < 0, "Setup failed");
}

static void teardown(void) {
  fail_if(csync_destroy(csync) < 0, "Teardown failed");
  csync = NULL;
  fail_if(system("rm -rf /tmp/check_csync1") < 0, "Teardown failed");
  fail_if(system("rm -rf /tmp/check_csync2") < 0, "Teardown failed");
}

START_TEST (check_csync_time)
{
  /*
   * The creation should took less than 1 second, so the return
   * value should be 0.
   */
  fail_unless(csync_timediff(csync) == 0, NULL);
}
END_TEST


static Suite *make_csync_suite(void) {
  Suite *s = suite_create("csync_time");

  create_case_fixture(s, "check_csync_time", check_csync_time, setup, teardown);

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

