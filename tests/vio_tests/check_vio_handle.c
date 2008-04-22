#define _GNU_SOURCE /* asprintf */
#include <string.h>

#include "support.h"

#include "vio/csync_vio_handle.h"
#include "vio/csync_vio_handle_private.h"

START_TEST (check_csync_vio_handle_new)
{
  int *number = NULL;
  csync_vio_handle_t *handle = NULL;

  number = c_malloc(sizeof(int));
  *number = 42;

  handle = csync_vio_handle_new("/tmp", (csync_vio_method_handle_t *) number);
  fail_if(handle == NULL, NULL);
  fail_unless(strcmp(handle->uri, "/tmp") == 0, NULL);

  SAFE_FREE(handle->method_handle);

  csync_vio_handle_destroy(handle);
}
END_TEST

START_TEST (check_csync_vio_handle_new_null)
{
  int *number = NULL;
  csync_vio_handle_t *handle = NULL;

  number = c_malloc(sizeof(int));
  *number = 42;

  handle = csync_vio_handle_new(NULL, (csync_vio_method_handle_t *) number);
  fail_unless(handle == NULL, NULL);

  handle = csync_vio_handle_new((char *) "/tmp", NULL);
  fail_unless(handle == NULL, NULL);

  SAFE_FREE(number);
}
END_TEST


static Suite *csync_vio_suite(void) {
  Suite *s = suite_create("csync_vio_handle");

  create_case(s, "check_csync_vio_handle_new", check_csync_vio_handle_new);
  create_case(s, "check_csync_vio_handle_new_null", check_csync_vio_handle_new_null);

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

