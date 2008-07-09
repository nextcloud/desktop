#define _GNU_SOURCE /* asprintf */
#include <string.h>
#include <unistd.h>

#include "support.h"

#define CSYNC_TEST 1
#include "csync_statedb.c"

CSYNC *csync;
const char *testdb = (char *) "/tmp/check_csync1/test.db";
const char *testtmpdb = (char *) "/tmp/check_csync1/test.db.ctmp";

static void setup(void) {
  fail_if(system("rm -rf /tmp/check_csync1") < 0, "Setup failed");
  fail_if(system("mkdir -p /tmp/check_csync1") < 0, "Setup failed");
  fail_if(csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2") < 0, "Setup failed");
  SAFE_FREE(csync->options.config_dir);
  csync->options.config_dir = c_strdup("/tmp/check_csync1/");
}

static void teardown(void) {
  fail_if(csync_destroy(csync) < 0, "Teardown failed");
  fail_if(system("rm -rf /tmp/check_csync1") < 0, "Teardown failed");
}

START_TEST (check_csync_statedb_check)
{
  fail_if(system("mkdir -p /tmp/check_csync1") < 0, NULL);

  /* old db */
  fail_if(system("echo \"SQLite format 2\" > /tmp/check_csync1/test.db") < 0, NULL);
  fail_unless(_csync_statedb_check(testdb) == 0);

  /* db already exists */
  fail_unless(_csync_statedb_check(testdb) == 0);

  /* no db exists */
  fail_if(system("rm -f /tmp/check_csync1/test.db") < 0, NULL);
  fail_unless(_csync_statedb_check(testdb) == 0);

  fail_unless(_csync_statedb_check((char *) "/tmp/check_csync1/") < 0);

  fail_if(system("rm -rf /tmp/check_csync1") < 0, NULL);
}
END_TEST

START_TEST (check_csync_statedb_load)
{
  struct stat sb;
  fail_unless(csync_statedb_load(csync, testdb) == 0, NULL);

  fail_unless(lstat(testtmpdb, &sb) == 0, NULL);

  sqlite3_close(csync->statedb.db);
}
END_TEST

START_TEST (check_csync_statedb_close)
{
  struct stat sb;
  time_t modtime;
  /* statedb not written */
  csync_statedb_load(csync, testdb);

  fail_unless(lstat(testdb, &sb) == 0, NULL);
  modtime = sb.st_mtime;

  fail_unless(csync_statedb_close(csync, testdb, 0) == 0, NULL);

  fail_unless(lstat(testdb, &sb) == 0, NULL);
  fail_unless(modtime == sb.st_mtime, NULL);

  csync_statedb_load(csync, testdb);

  fail_unless(lstat(testdb, &sb) == 0, NULL);
  modtime = sb.st_mtime;

  /* wait a sec or the modtime will be the same */
  sleep(1);

  /* statedb written */
  fail_unless(csync_statedb_close(csync, testdb, 1) == 0, NULL);

  fail_unless(lstat(testdb, &sb) == 0, NULL);
  fail_unless(modtime < sb.st_mtime, NULL);
}
END_TEST

static Suite *make_csync_suite(void) {
  Suite *s = suite_create("csync_statedb");

  create_case(s, "check_csync_statedb_check", check_csync_statedb_check);
  create_case_fixture(s, "check_csync_statedb_load", check_csync_statedb_load, setup, teardown);
  create_case_fixture(s, "check_csync_statedb_close", check_csync_statedb_close, setup, teardown);

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

