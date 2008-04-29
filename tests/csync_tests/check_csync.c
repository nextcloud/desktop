#include <string.h>

#include "support.h"

#define CSYNC_TEST 1
#include "csync_private.h"

CSYNC *csync;

static void setup(void) {
  csync_create(&csync, "/tmp/csync1", "/tmp/csync2");
}

static void teardown(void) {
  csync_destroy(csync);
}

START_TEST (check_csync_destroy_null)
{
  fail_unless(csync_destroy(NULL) < 0, NULL);
}
END_TEST

START_TEST (check_csync_create)
{
  fail_unless(csync_create(&csync, "/tmp/csync1", "/tmp/csync2") == 0, NULL);

  fail_unless(csync->options.max_depth == MAX_DEPTH, NULL);
  fail_unless(csync->options.max_time_difference == MAX_TIME_DIFFERENCE, NULL);
  fail_unless(strcmp(csync->options.config_dir, CSYNC_CONF_DIR) > 0, NULL);

  fail_unless(csync_destroy(csync) == 0, NULL);
}
END_TEST

START_TEST (check_csync_init)
{
  fail_unless(csync_init(csync) == 0, NULL);

  fail_unless((csync->status & CSYNC_INIT) == 1, NULL);

  fail_unless(csync_init(csync) == 1, NULL);
}
END_TEST

static Suite *csync_suite(void) {
  Suite *s = suite_create("csync");

  create_case(s, "check_csync_destroy_null", check_csync_destroy_null);
  create_case(s, "check_csync_create", check_csync_create);
  create_case_fixture(s, "check_csync_init", check_csync_init, setup, teardown);

  return s;
}

int main(void) {
  int nf;

  Suite *s = csync_suite();

  SRunner *sr;
  sr = srunner_create(s);
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

