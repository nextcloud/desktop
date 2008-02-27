#include <string.h>

#include "support.h"

#include "csync_private.h"

CSYNC *csync;

static void setup(void) {
  csync_create(&csync);
}

static void teardown(void) {
  csync_destroy(csync);
}

START_TEST (csync_create_test)
{
  fail_unless(csync_create(&csync) == 0, NULL);

  fail_unless(csync->options.max_depth == MAX_DEPTH, NULL);
  fail_unless(csync->options.max_time_difference == MAX_TIME_DIFFERENCE, NULL);
  fail_unless(strcmp(csync->options.config_dir, CSYNC_CONF_DIR) > 0, NULL);

  csync->destroy(csync);
}
END_TEST

START_TEST (csync_init_test)
{
  fail_unless(csync->init(csync) == 0, NULL);

  fail_unless(csync->internal->_initialized == 1, NULL);

  fail_unless(csync->init(csync) == 1, NULL);
}
END_TEST

static Suite *csync_suite(void) {
  Suite *s = suite_create("csync");

  create_case(s, "csync_create_test", csync_create_test);
  create_case_fixture(s, "csync_init_test", csync_init_test, setup, teardown);

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

