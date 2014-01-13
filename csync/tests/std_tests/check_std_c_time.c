#include <string.h>
#include <unistd.h>

#include "torture.h"

#include "csync_time.h"
#include "std/c_time.h"

static void check_c_tspecdiff(void **state)
{
    struct timespec start, finish, diff;

    (void) state; /* unused */

    csync_gettime(&start);
    csync_gettime(&finish);

    diff = c_tspecdiff(finish, start);

    assert_int_equal(diff.tv_sec, 0);
    assert_true(diff.tv_nsec >= 0);
}

static void check_c_tspecdiff_five(void **state)
{
    struct timespec start, finish, diff;

    (void) state; /* unused */

    csync_gettime(&start);
    sleep(5);
    csync_gettime(&finish);

    diff = c_tspecdiff(finish, start);

    assert_int_equal(diff.tv_sec, 5);
    assert_true(diff.tv_nsec > 0);
}

static void check_c_secdiff(void **state)
{
    struct timespec start, finish;
    double diff;

    (void) state; /* unused */

    csync_gettime(&start);
    csync_gettime(&finish);

    diff = c_secdiff(finish, start);

    assert_true(diff >= 0.00 && diff < 1.00);
}

static void check_c_secdiff_three(void **state)
{
    struct timespec start, finish;
    double diff;

    (void) state; /* unused */

    csync_gettime(&start);
    sleep(3);
    csync_gettime(&finish);

    diff = c_secdiff(finish, start);

    assert_true(diff > 3.00 && diff < 4.00);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test(check_c_tspecdiff),
        unit_test(check_c_tspecdiff_five),
        unit_test(check_c_secdiff),
        unit_test(check_c_secdiff_three),
    };

    return run_tests(tests);
}

