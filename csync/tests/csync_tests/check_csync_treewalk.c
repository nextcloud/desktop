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

#define CSYNC_TEST 1
#include "csync_private.h"

static void setup_local(void **state) {
    CSYNC *csync;
    int rc;

    rc = system("mkdir -p /tmp/check_csync1");
    assert_int_equal(rc, 0);

    rc = system("mkdir -p /tmp/check_csync2");
    assert_int_equal(rc, 0);

    rc = system("mkdir -p /tmp/check_csync");
    assert_int_equal(rc, 0);

    rc = system("echo \"This is test data\" > /tmp/check_csync1/testfile1.txt");
    assert_int_equal(rc, 0);

    rc = system("echo \"This is also test data\" > /tmp/check_csync1/testfile2.txt");
    assert_int_equal(rc, 0);

    rc = csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2");
    assert_int_equal(rc, 0);

    rc = csync_set_config_dir(csync, "/tmp/check_csync/");
    assert_int_equal(rc, 0);

    rc = csync_init(csync);
    assert_int_equal(rc, 0);

    *state = csync;
}

static void teardown_local(void **state) {
    CSYNC *csync = (CSYNC *)*state;

    csync_destroy(csync);
    system("rm -rf /tmp/check_csync1");
    system("rm -rf /tmp/check_csync2");
    system("rm -rf /tmp/check_csync");
}

static int visitor(TREE_WALK_FILE* file, void *userdata)
{
    int *file_count;

    assert_non_null(userdata);
    assert_non_null(file);

    file_count = (int *)userdata;

    (*file_count)++;

    return 0;
}

static void check_csync_treewalk_local(void **state)
{
    CSYNC *csync = (CSYNC *)*state;
    int file_count = 0;
    int rc;

    csync_set_userdata(csync, (void *)&file_count);

    rc = csync_walk_local_tree(csync, &visitor, 0);
    assert_int_equal(rc, 0);
    assert_int_equal(file_count, 0);

    rc = csync_update(csync);
    assert_int_equal(rc, 0);

    rc = csync_walk_local_tree(csync, &visitor, 0);
    assert_int_equal(rc, 0);
    assert_int_equal(file_count, 2);
}

static void check_csync_treewalk_local_with_filter(void **state)
{
    CSYNC *csync = (CSYNC *)*state;
    int file_count = 0;
    int rc;

    csync_set_userdata(csync, &file_count);

    rc = csync_walk_local_tree(csync, &visitor, 0);
    assert_int_equal(rc, 0);
    assert_int_equal(file_count, 0);

    rc = csync_update(csync);
    assert_int_equal(rc, 0);

    rc = csync_walk_local_tree(csync, &visitor, CSYNC_INSTRUCTION_NEW);
    assert_int_equal(rc, 0);
    assert_int_equal(file_count, 2 );

    file_count = 0;
    rc = csync_walk_local_tree(csync,
                               &visitor,
                               CSYNC_INSTRUCTION_NEW|CSYNC_INSTRUCTION_REMOVE);
    assert_int_equal(rc, 0);
    assert_int_equal(file_count, 2);

    file_count = 0;
    rc = csync_walk_local_tree(csync, &visitor, CSYNC_INSTRUCTION_RENAME);
    assert_int_equal(rc, 0);
    assert_int_equal(file_count, 0);
}

int torture_run_tests(void)
{
  const UnitTest tests[] = {
    unit_test_setup_teardown(check_csync_treewalk_local, setup_local, teardown_local ),
    unit_test_setup_teardown(check_csync_treewalk_local_with_filter, setup_local, teardown_local)
  };

  return run_tests(tests);
}
