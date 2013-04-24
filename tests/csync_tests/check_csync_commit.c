#include <string.h>

#include "torture.h"

#include "csync_private.h"

static void setup(void **state) {
    CSYNC *csync;
    int rc;

    rc = system("mkdir -p /tmp/check_csync1");
    assert_int_equal(rc, 0);

    rc = system("mkdir -p /tmp/check_csync2");
    assert_int_equal(rc, 0);

    rc = csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2");
    assert_int_equal(rc, 0);

    rc = csync_set_config_dir(csync, "/tmp/check_csync");
    assert_int_equal(rc, 0);

    *state = csync;
}

static void setup_module(void **state) {
    CSYNC *csync;
    int rc;

    rc = system("mkdir -p /tmp/check_csync1");
    assert_int_equal(rc, 0);

    rc = system("mkdir -p /tmp/check_csync2");
    assert_int_equal(rc, 0);

    rc = csync_create(&csync, "/tmp/check_csync1", "dummy://foo/bar");
    assert_int_equal(rc, 0);

    rc = csync_set_config_dir(csync, "/tmp/check_csync");
    assert_int_equal(rc, 0);

    rc = csync_init(csync);
    *state = csync;
}

static void teardown(void **state) {
    CSYNC *csync = *state;
    int rc;

    rc = csync_destroy(csync);

    rc = system("rm -rf /tmp/check_csync");
    assert_int_equal(rc, 0);

    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);

    rc = system("rm -rf /tmp/check_csync2");
    assert_int_equal(rc, 0);

    *state = NULL;
}

static void check_csync_commit_null(void **state)
{
    int rc;

    (void) state; /* unused */

    rc = csync_commit(NULL);
    assert_int_equal(rc, -1);
}

static void check_csync_commit(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_commit(csync);
    assert_int_equal(rc, 0);

    assert_int_equal(csync->status & CSYNC_STATUS_INIT, 1);
}

static void check_csync_commit_dummy(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_commit(csync);
    assert_int_equal(rc, 0);

    assert_int_equal(csync->status & CSYNC_STATUS_INIT, 1);

}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_csync_commit_null, setup, teardown),
        unit_test_setup_teardown(check_csync_commit, setup, teardown),
        unit_test_setup_teardown(check_csync_commit_dummy, setup_module, teardown),
    };

    return run_tests(tests);
}
