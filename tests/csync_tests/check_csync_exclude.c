#define _GNU_SOURCE /* asprintf */
#include <string.h>

#include "support.h"

#include "csync_exclude.c"

CSYNC *csync;

static void setup(void) {
  csync_create(&csync, "/tmp/csync1", "/tmp/csync2");
  SAFE_FREE(csync->options.config_dir);
  csync->options.config_dir = c_strdup("/tmp/check_csync/");
}

static void setup_init(void) {
  csync_create(&csync, "/tmp/csync1", "/tmp/csync2");
  SAFE_FREE(csync->options.config_dir);
  csync->options.config_dir = c_strdup("/tmp/check_csync/");
  csync_exclude_load(csync, SYSCONFDIR "/config/" CSYNC_EXCLUDE_FILE);
}

static void teardown(void) {
  csync_destroy(csync);
  system("rm -rf /tmp/check_csync");
}

START_TEST (check_csync_exclude_add)
{
  csync_exclude_add(csync, (const char *) "/tmp/check_csync/*");
  fail_unless(strcmp(csync->excludes->vector[0], (const char *) "/tmp/check_csync/*") == 0, NULL);
}
END_TEST

START_TEST (check_csync_exclude_load)
{
  fail_unless(csync_exclude_load(csync, SYSCONFDIR "/config/" CSYNC_EXCLUDE_FILE) == 0, NULL);
  fail_unless(strcmp(csync->excludes->vector[0], (const char *) ".kde*/cache-*") == 0, NULL);
}
END_TEST

START_TEST (check_csync_excluded)
{
  fail_unless(csync_excluded(csync, ".kde4/cache-maximegalon/") == 1, NULL);
  fail_unless(csync_excluded(csync, ".mozilla/plugins/foo.so") == 1, NULL);
}
END_TEST


static Suite *csync_suite(void) {
  Suite *s = suite_create("csync_exclude");

  create_case_fixture(s, "check_csync_exclude_add", check_csync_exclude_add, setup, teardown);
  create_case_fixture(s, "check_csync_exclude_load", check_csync_exclude_load, setup, teardown);
  create_case_fixture(s, "check_csync_excluded", check_csync_excluded, setup_init, teardown);

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

