#include <string.h>
#include <unistd.h>

#include "torture.h"

#define CSYNC_TEST 1
#include "csync_statedb.c"

#define TESTDB "/tmp/check_csync1/test.db"
#define TESTDBTMP "/tmp/check_csync1/test.db.ctmp"

static void setup(void **state) {
    CSYNC *csync;
    int rc;

    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);

    rc = system("mkdir -p /tmp/check_csync1");
    assert_int_equal(rc, 0);

    rc = csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2");
    assert_int_equal(rc, 0);

    free(csync->options.config_dir);
    csync->options.config_dir = c_strdup("/tmp/check_csync1/");

    *state = csync;
}

static void teardown(void **state) {
    CSYNC *csync = *state;
    int rc;

    rc = csync_destroy(csync);
    assert_int_equal(rc, 0);

    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);

    *state = NULL;
}

static void check_csync_statedb_check(void **state)
{
    int rc;

    (void) state; /* unused */

    rc = system("mkdir -p /tmp/check_csync1");

    /* old db */
    rc = system("echo \"SQLite format 2\" > /tmp/check_csync1/test.db");
    assert_int_equal(rc, 0);
    rc = _csync_statedb_check(NULL, TESTDB);
    assert_int_equal(rc, 0);

    /* db already exists */
    rc = _csync_statedb_check(NULL, TESTDB);
    assert_int_equal(rc, 0);

    /* no db exists */
    rc = system("rm -f /tmp/check_csync1/test.db");
    assert_int_equal(rc, 0);
    rc = _csync_statedb_check(NULL, TESTDB);
    assert_int_equal(rc, 0);

    rc = _csync_statedb_check(NULL, "/tmp/check_csync1/");
    assert_int_equal(rc, -1);

    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);
}

static void check_csync_statedb_load(void **state)
{
    CSYNC *csync = *state;
    struct stat sb;
    int rc;

    rc = csync_statedb_load(csync, TESTDB);
    assert_int_equal(rc, 0);

    rc = lstat(TESTDBTMP, &sb);
    assert_int_equal(rc, 0);

    sqlite3_close(csync->statedb.db);
}

static void check_csync_statedb_close(void **state)
{
    CSYNC *csync = *state;
    struct stat sb;
    time_t modtime;
    int rc;

    /* statedb not written */
    csync_statedb_load(csync, TESTDB);

    rc = lstat(TESTDB, &sb);
    assert_int_equal(rc, 0);
    modtime = sb.st_mtime;

    rc = csync_statedb_close(csync, TESTDB, 0);
    assert_int_equal(rc, 0);

    rc = lstat(TESTDB, &sb);
    assert_int_equal(rc, 0);
    assert_int_equal(modtime, sb.st_mtime);

    csync_statedb_load(csync, TESTDB);

    rc = lstat(TESTDB, &sb);
    assert_int_equal(rc, 0);
    modtime = sb.st_mtime;

    /* wait a sec or the modtime will be the same */
    sleep(1);

    /* statedb written */
    rc = csync_statedb_close(csync, TESTDB, 1);
    assert_int_equal(rc, 0);

    rc = lstat(TESTDB, &sb);
    assert_int_equal(rc, 0);
    assert_true(modtime < sb.st_mtime);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test(check_csync_statedb_check),
        unit_test_setup_teardown(check_csync_statedb_load, setup, teardown),
        unit_test_setup_teardown(check_csync_statedb_close, setup, teardown),
    };

    return run_tests(tests);
}

