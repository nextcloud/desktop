#include <string.h>

#include "torture.h"

#define CSYNC_TEST 1
#include "csync.h"
#include "csync_config.c"

#define TESTCONF "/tmp/check_csync1/csync.conf"

static void setup(void **state) {
    CSYNC *csync;
    int rc;

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

    rc = system("rm -rf /tmp/check_csync2");
    assert_int_equal(rc, 0);

    *state = NULL;
}

static void check_csync_config_copy_default(void **state)
{
    int rc;

    (void) state; /* unused */

    rc = _csync_config_copy_default(TESTCONF);
    assert_int_equal(rc, 0);
}

static void check_csync_config_load(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = _csync_config_copy_default(TESTCONF);
    assert_int_equal(rc, 0);

    rc = csync_config_parse_file(csync, TESTCONF);
    assert_int_equal(rc, 0);

    assert_false(csync->options.with_conflict_copys);
    assert_int_equal(csync->options.max_time_difference, 10);
    assert_int_equal(csync->options.max_depth, 50);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_csync_config_copy_default, setup, teardown),
        unit_test_setup_teardown(check_csync_config_load, setup, teardown),
    };

    return run_tests(tests);
}

