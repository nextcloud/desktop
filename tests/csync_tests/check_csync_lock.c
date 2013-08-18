#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "torture.h"

#define CSYNC_TEST 1
#include "std/c_file.h"
#include "csync_lock.h"

#define TEST_LOCK "/tmp/check_csync/test"

static void setup(void **state) {
    int rc;

    (void) state; /* unused */

    rc = system("mkdir -p /tmp/check_csync");
    assert_int_equal(rc, 0);
}

static void teardown(void **state) {
    int rc;

    (void) state; /* unused */

    rc = system("rm -rf /tmp/check_csync");
    assert_int_equal(rc, 0);
}

static void check_csync_lock(void **state)
{
    int rc;

    (void) state; /* unused */

    rc = csync_lock(TEST_LOCK);
    assert_int_equal(rc, 0);

    assert_true(c_isfile(TEST_LOCK));

    rc = csync_lock(TEST_LOCK);
    assert_int_equal(rc, -1);

    csync_lock_remove(TEST_LOCK);
    assert_false(c_isfile(TEST_LOCK));
}

static void check_csync_lock_content(void **state)
{
    char buf[8] = {0};
    int  fd, pid, rc;

    (void) state; /* unused */

    rc = csync_lock(TEST_LOCK);
    assert_int_equal(rc, 0);

    assert_true(c_isfile(TEST_LOCK));

    /* open lock file */
    fd = open(TEST_LOCK, O_RDONLY);
    assert_true(fd > 0);

    /* read content */
    pid = read(fd, buf, sizeof(buf));
    close(fd);

    assert_true(pid > 0);

    /* get pid */
    buf[sizeof(buf) - 1] = '\0';
    pid = strtol(buf, NULL, 10);

    assert_int_equal(pid, getpid());

    csync_lock_remove(TEST_LOCK);
    assert_false(c_isfile(TEST_LOCK));
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_csync_lock, setup, teardown),
        unit_test_setup_teardown(check_csync_lock_content, setup, teardown),
    };

    return run_tests(tests);
}

