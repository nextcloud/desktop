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

static void check_csync_init_null(void **state)
{
    int rc;

    (void) state; /* unused */

    rc = csync_init(NULL);
    assert_int_equal(rc, -1);
}

static void check_csync_init(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_init(csync);
    assert_int_equal(rc, 0);

    assert_int_equal(csync->status & CSYNC_STATUS_INIT, 1);

    rc = csync_init(csync);
    assert_int_equal(rc, 1);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_csync_init_null, setup, teardown),
        unit_test_setup_teardown(check_csync_init, setup, teardown),
        unit_test_setup_teardown(check_csync_init, setup_module, teardown),
    };

    return run_tests(tests);
}

