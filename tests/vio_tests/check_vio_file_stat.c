#include "support.h"

#include "vio/csync_vio_file_stat_private.h"

START_TEST (check_csync_vio_file_stat_new)
{
  csync_vio_file_stat_t *stat = NULL;

  stat = csync_vio_file_stat_new();
  fail_if(stat == NULL, NULL);

  csync_vio_file_stat_destroy(stat);
}
END_TEST


static Suite *csync_vio_suite(void) {
  Suite *s = suite_create("csync_vio_file_stat");

  create_case(s, "check_csync_vio_file_stat_new", check_csync_vio_file_stat_new);

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

