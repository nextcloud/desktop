#include <string.h>

#include "support.h"

#include "csync_private.h"
#include "csync_journal.h"
#include "csync_log.h"

CSYNC *csync;

static void setup(void) {
  csync_create(&csync);
  SAFE_FREE(csync->options.config_dir);
  csync->options.config_dir = c_strdup("/tmp/check_csync/");
  csync_init(csync);
}

static void teardown(void) {
  csync_destroy(csync);
  system("rm -rf /tmp/check_csync");
}

START_TEST (check_csync_journal_query_create_and_insert_table)
{
  c_strlist_t *result = NULL;
  result = csync_journal_query(csync, "CREATE TABLE test(key INTEGER, text VARCHAR(10));");
  c_strlist_destroy(result);
  fail_unless(csync_journal_insert(csync, "INSERT INTO test (key, text) VALUES (42, 'hello');"), NULL);
  result = csync_journal_query(csync, "SELECT * FROM test;");
  fail_unless(result->count == 2, NULL);
  fail_unless(strcmp(result->vector[0], "42") == 0, NULL);
  fail_unless(strcmp(result->vector[1], "hello") == 0, NULL);
  c_strlist_destroy(result);
}
END_TEST


static Suite *csync_suite(void) {
  Suite *s = suite_create("csync_journal");

  create_case_fixture(s, "check_csync_journal_query_create_and_insert_table", check_csync_journal_query_create_and_insert_table, setup, teardown);

  return s;
}

int main(void) {
  int nf;

  Suite *s = csync_suite();

  SRunner *sr;
  sr = srunner_create(s);
  srunner_set_fork_status(sr, CK_NOFORK);
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

