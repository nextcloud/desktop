#define _GNU_SOURCE /* asprintf */
#include <string.h>

#include "support.h"
#include "config.h"

#define CSYNC_TEST 1
#include "csync_exclude.c"

CSYNC *csync;

static void setup(void) {
  fail_if(csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2") < 0, "Setup failed");
  SAFE_FREE(csync->options.config_dir);
  csync->options.config_dir = c_strdup("/tmp/check_csync1/");
}

static void setup_init(void) {
  fail_if(csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2") < 0, "Setup failed");
  SAFE_FREE(csync->options.config_dir);
  csync->options.config_dir = c_strdup("/tmp/check_csync1");
  fail_if(csync_exclude_load(csync, BINARYDIR "/config/" CSYNC_EXCLUDE_FILE) < 0, "Setup failed");
}

static void teardown(void) {
  fail_if(csync_destroy(csync) < 0, "Teardown failed");
  fail_if (system("rm -rf /tmp/check_csync1") < 0, "Teardown failed");
}

START_TEST (check_csync_exclude_add)
{
  csync_exclude_add(csync, (const char *) "/tmp/check_csync1/*");
  fail_unless(strcmp(csync->excludes->vector[0], (const char *) "/tmp/check_csync1/*") == 0, NULL);
}
END_TEST

START_TEST (check_csync_exclude_load)
{
  fail_unless(csync_exclude_load(csync, BINARYDIR "/config/" CSYNC_EXCLUDE_FILE) == 0, NULL);
  fail_unless(strcmp(csync->excludes->vector[0], (const char *) ".ccache/*") == 0, NULL);
}
END_TEST

START_TEST (check_csync_excluded)
{
  fail_unless(csync_excluded(csync, ".kde4/cache-maximegalon/") == 1, NULL);
  fail_unless(csync_excluded(csync, ".mozilla/plugins/foo.so") == 1, NULL);
}
END_TEST


static Suite *make_csync_suite(void) {
  Suite *s = suite_create("csync_exclude");

  create_case_fixture(s, "check_csync_exclude_add", check_csync_exclude_add, setup, teardown);
  create_case_fixture(s, "check_csync_exclude_load", check_csync_exclude_load, setup, teardown);
  create_case_fixture(s, "check_csync_excluded", check_csync_excluded, setup_init, teardown);

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

