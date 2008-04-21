#define _GNU_SOURCE /* asprintf */
#include <string.h>

#include "support.h"

#include "csync_private.h"
#include "vio/csync_vio.h"

CSYNC *csync;

static void setup(void) {
  csync_create(&csync);
  csync_init(csync);
}

static void teardown(void) {
  csync_destroy(csync);
}

START_TEST (check_csync_vio_load)
{
  fail_unless(csync_vio_init(csync, "smb", NULL) == 0, NULL);

  csync_vio_shutdown(csync);
}
END_TEST

static Suite *csync_vio_suite(void) {
  Suite *s = suite_create("csync_vio");

  create_case_fixture(s, "check_csync_vio_load", check_csync_vio_load, setup, teardown);

  return s;
}

int main(void) {
  int nf;

  Suite *s = csync_vio_suite();

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

