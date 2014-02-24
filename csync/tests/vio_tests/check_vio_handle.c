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

#include "torture.h"

#include "vio/csync_vio_handle.h"
#include "vio/csync_vio_handle_private.h"

static void check_csync_vio_handle_new(void **state)
{
    int *number;
    csync_vio_handle_t *handle;

    (void) state; /* unused */

    number = malloc(sizeof(int));
    *number = 42;

    handle = csync_vio_handle_new("/tmp", (csync_vio_method_handle_t *) number);
    assert_non_null(handle);
    assert_string_equal(handle->uri, "/tmp");

    free(handle->method_handle);

    csync_vio_handle_destroy(handle);
}

static void check_csync_vio_handle_new_null(void **state)
{
    int *number;
    csync_vio_handle_t *handle;

    (void) state; /* unused */

    number = malloc(sizeof(int));
    *number = 42;

    handle = csync_vio_handle_new(NULL, (csync_vio_method_handle_t *) number);
    assert_null(handle);

    handle = csync_vio_handle_new((char *) "/tmp", NULL);
    assert_null(handle);

    free(number);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test(check_csync_vio_handle_new),
        unit_test(check_csync_vio_handle_new_null),
    };

    return run_tests(tests);
}

