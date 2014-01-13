#include "torture.h"

#include "csync_time.h"

static void setup(void **state) {
    CSYNC *csync;
    int rc;

    rc = system("mkdir -p /tmp/check_csync1");
    assert_int_equal(rc, 0);
    rc = system("mkdir -p /tmp/check_csync2");
    assert_int_equal(rc, 0);
    rc = csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2");
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
    rc = system("rm -rf /tmp/check_csync2");
    assert_int_equal(rc, 0);
}

static void check_csync_time(void **state)
{
    CSYNC *csync = *state;
    /*
     * The creation should took less than 1 second, so the return
     * value should be 0.
     */
    assert_int_equal(csync_timediff(csync), 0);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_csync_time, setup, teardown),
    };

    return run_tests(tests);
}

