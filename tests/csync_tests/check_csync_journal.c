#define _GNU_SOURCE /* asprintf */
#include <string.h>

#include "support.h"

#define CSYNC_TEST 1
#include "csync_journal.c"

CSYNC *csync;
const char *testdb = (char *) "/tmp/check_csync/test.db";

static void setup(void) {
  csync_create(&csync, "/tmp/csync1", "/tmp/csync2");
  SAFE_FREE(csync->options.config_dir);
  csync->options.config_dir = c_strdup("/tmp/check_csync/");
}

static void setup_init(void) {
  csync_create(&csync, "/tmp/csync1", "/tmp/csync2");
  SAFE_FREE(csync->options.config_dir);
  csync->options.config_dir = c_strdup("/tmp/check_csync/");
  csync_init(csync);
}

static void teardown(void) {
  csync_destroy(csync);
  system("rm -rf /tmp/check_csync");
}

START_TEST (check_csync_journal_check)
{
  system("mkdir -p /tmp/check_csync");

  /* old db */
  system("echo \"SQLite format 2\" > /tmp/check_csync/test.db");
  fail_unless(csync_journal_check(testdb) == 0);

  /* db already exists */
  fail_unless(csync_journal_check(testdb) == 0);

  /* no db exists */
  system("rm -f /tmp/check_csync/test.db");
  fail_unless(csync_journal_check(testdb) == 0);

  fail_unless(csync_journal_check((char *) "/tmp/check_csync/") < 0);

  system("rm -rf /tmp/check_csync");
}
END_TEST

START_TEST (check_csync_journal_load)
{
  fail_unless(csync_journal_load(csync, testdb) == 0, NULL);
}
END_TEST

START_TEST (check_csync_journal_query_statement)
{
  c_strlist_t *result = NULL;
  result = csync_journal_query(csync, "");
  fail_unless(result == NULL, NULL);
  c_strlist_destroy(result);

  result = csync_journal_query(csync, "SELECT;");
  fail_unless(result != NULL, NULL);
  c_strlist_destroy(result);
}
END_TEST

START_TEST (check_csync_journal_insert_statement)
{
  c_strlist_t *result = NULL;
  result = csync_journal_query(csync, "CREATE TABLE test(key INTEGER, text VARCHAR(10));");
  c_strlist_destroy(result);
  fail_unless(csync_journal_insert(csync, "INSERT;") == 0, NULL);
  fail_unless(csync_journal_insert(csync, "INSERT") == 0, NULL);
  fail_unless(csync_journal_insert(csync, "") == 0, NULL);
}
END_TEST

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

START_TEST (check_csync_journal_is_empty)
{
  c_strlist_t *result = NULL;

  /* we have an empty db */
  fail_unless(csync_journal_is_empty(csync) == 1, NULL);

  /* add a table and an entry */
  result = csync_journal_query(csync, "CREATE TABLE metadata(key INTEGER, text VARCHAR(10));");
  c_strlist_destroy(result);
  fail_unless(csync_journal_insert(csync, "INSERT INTO metadata (key, text) VALUES (42, 'hello');"), NULL);

  fail_unless(csync_journal_is_empty(csync) == 0, NULL);
}
END_TEST

START_TEST (check_csync_journal_create_tables)
{
  fail_unless(csync_journal_create_tables(csync) == 0, NULL);
}
END_TEST

static Suite *csync_suite(void) {
  Suite *s = suite_create("csync_journal");

  create_case(s, "check_csync_journal_check", check_csync_journal_check);
  create_case_fixture(s, "check_csync_journal_load", check_csync_journal_load, setup, teardown);
  create_case_fixture(s, "check_csync_journal_query_statement", check_csync_journal_query_statement, setup_init, teardown);
  create_case_fixture(s, "check_csync_journal_insert_statement", check_csync_journal_insert_statement, setup_init, teardown);
  create_case_fixture(s, "check_csync_journal_query_create_and_insert_table", check_csync_journal_query_create_and_insert_table, setup_init, teardown);
  create_case_fixture(s, "check_csync_journal_is_empty", check_csync_journal_is_empty, setup_init, teardown);
  create_case_fixture(s, "check_csync_journal_create_tables", check_csync_journal_create_tables, setup_init, teardown);

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

