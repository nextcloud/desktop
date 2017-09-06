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
#include <sys/types.h>
#include <sys/stat.h>

#include "torture.h"

#include "csync.h"
#include "csync_log.cpp"
#include "c_private.h"
#include "std/c_utf8.h"

static int setup(void **state) {
    int rc;

    rc = system("mkdir -p /tmp/check_csync1");
    assert_int_equal(rc, 0);

    *state = NULL;

    return 0;
}

static int teardown(void **state) {
    int rc;

    rc = system("rm -rf /tmp/check_csync");
    assert_int_equal(rc, 0);

    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);

    *state = NULL;
    
    return 0;
}

static void check_log_callback(int verbosity,
                               const char *function,
                               const char *buffer)
{
    int rc;

    assert_true(verbosity >= 0);
    assert_non_null(function);
    assert_false(function[0] == '\0');
    assert_non_null(buffer);
    assert_false(buffer[0] == '\0');

    rc = system("touch /tmp/check_csync1/cb_called");
    assert_int_equal(rc, 0);
}

static void check_set_log_level(void **state)
{
    int rc;

    (void) state;

    rc = csync_set_log_level(-5);
    assert_int_equal(rc, -1);

    rc = csync_set_log_level(5);
    assert_int_equal(rc, 0);

    rc = csync_get_log_level();
    assert_int_equal(rc, 5);
}

static void check_set_auth_callback(void **state)
{
    csync_log_callback log_fn;
    int rc;

    (void) state;

    rc = csync_set_log_callback(NULL);
    assert_int_equal(rc, -1);

    rc = csync_set_log_callback(check_log_callback);
    assert_int_equal(rc, 0);

    log_fn = csync_get_log_callback();
    assert_non_null(log_fn);
    assert_true(log_fn == &check_log_callback);
}

static void check_logging(void **state)
{
    int rc;
    csync_stat_t sb;
    mbchar_t *path;
    path = c_utf8_path_to_locale("/tmp/check_csync1/cb_called");

    (void) state; /* unused */

    assert_non_null(path);

    rc = csync_set_log_level(1);
    assert_int_equal(rc, 0);

    rc = csync_set_log_callback(check_log_callback);
    assert_int_equal(rc, 0);

    csync_log(1, __func__, "rc = %d", rc);

    rc = _tstat(path, &sb);

    c_free_locale_string(path);

    assert_int_equal(rc, 0);
}

int torture_run_tests(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(check_set_log_level),
        cmocka_unit_test(check_set_auth_callback),
        cmocka_unit_test_setup_teardown(check_logging, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
