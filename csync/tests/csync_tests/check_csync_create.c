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
#include <stdlib.h>
#include <stdio.h>

#include "torture.h"

#include "csync_private.h"


static void check_csync_destroy_null(void **state)
{
    int rc;

    (void) state; /* unused */

    rc = csync_destroy(NULL);
    assert_int_equal(rc, -1);
}

static void check_csync_create(void **state)
{
    CSYNC *csync;
    char confdir[1024] = {0};
    int rc;

    (void) state; /* unused */

    rc = csync_create(&csync, "/tmp/csync1", "/tmp/csync2");
    assert_int_equal(rc, 0);

    assert_int_equal(csync->options.max_depth, MAX_DEPTH);
    assert_int_equal(csync->options.max_time_difference, MAX_TIME_DIFFERENCE);

    snprintf(confdir, sizeof(confdir), "%s/%s", getenv("HOME"), CSYNC_CONF_DIR);
    assert_string_equal(csync->options.config_dir, confdir);

    rc = csync_destroy(csync);
    assert_int_equal(rc, 0);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test(check_csync_destroy_null),
        unit_test(check_csync_create),
    };

    return run_tests(tests);
}

