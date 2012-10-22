#include "config.h"
#include <string.h>

#include "torture.h"

#define CSYNC_TEST 1
#include "csync_exclude.c"

static void setup(void **state) {
    CSYNC *csync;
    int rc;

    rc = csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2");
    assert_int_equal(rc, 0);

    free(csync->options.config_dir);
    csync->options.config_dir = c_strdup("/tmp/check_csync1/");
    assert_non_null(csync->options.config_dir);

    *state = csync;
}

static void setup_init(void **state) {
    CSYNC *csync;
    int rc;

    rc = csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2");
    assert_int_equal(rc, 0);

    free(csync->options.config_dir);
    csync->options.config_dir = c_strdup("/tmp/check_csync1/");
    assert_non_null(csync->options.config_dir);

    rc = csync_exclude_load(csync, BINARYDIR "/config/" CSYNC_EXCLUDE_FILE);
    assert_int_equal(rc, 0);

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

static void check_csync_exclude_add(void **state)
{
    CSYNC *csync = *state;

    _csync_exclude_add(csync, (const char *) "/tmp/check_csync1/*");
    assert_string_equal(csync->excludes->vector[0], "/tmp/check_csync1/*");
}

static void check_csync_exclude_load(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_exclude_load(csync, BINARYDIR "/config/" CSYNC_EXCLUDE_FILE);
    assert_int_equal(rc, 0);

    assert_string_equal(csync->excludes->vector[0], ".beagle");
}

static void check_csync_excluded(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_excluded(csync, ".kde/share/config/kwin.eventsrc");
    assert_int_equal(rc, 0);
    rc = csync_excluded(csync, ".kde4/cache-maximegalon/cache1.txt");
    assert_int_equal(rc, 1);
    rc = csync_excluded(csync, ".mozilla/plugins");
    assert_int_equal(rc, 1);

    /*
     * Test for patterns in subdirs. '.beagle' is defined as a pattern and has
     * to be found in top dir as well as in directories underneath.
     */
    rc = csync_excluded(csync, ".beagle");
    assert_int_equal(rc, 1);
    rc = csync_excluded(csync, "foo/.beagle");
    assert_int_equal(rc, 1);
    rc = csync_excluded(csync, "foo/bar/.beagle");
    assert_int_equal(rc, 1);

    rc = csync_excluded(csync, ".csync_journal.db");
    assert_int_equal(rc, 1);
    rc = csync_excluded(csync, ".csync_journal.db.ctmp");
    assert_int_equal(rc, 1);
    rc = csync_excluded(csync, "subdir/.csync_journal.db");
    assert_int_equal(rc, 1);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_csync_exclude_add, setup, teardown),
        unit_test_setup_teardown(check_csync_exclude_load, setup, teardown),
        unit_test_setup_teardown(check_csync_excluded, setup_init, teardown),
    };

    return run_tests(tests);
}
