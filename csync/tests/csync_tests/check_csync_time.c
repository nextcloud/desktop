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

