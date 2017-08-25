/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <string.h>

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
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(check_c_tspecdiff),
        cmocka_unit_test(check_c_tspecdiff_five),
        cmocka_unit_test(check_c_secdiff),
        cmocka_unit_test(check_c_secdiff_three),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

