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
#include "config_csync.h"
#include <string.h>

#include "torture.h"

#define CSYNC_TEST 1
#include "csync_exclude.c"

#define EXCLUDE_LIST_FILE SOURCEDIR"/../sync-exclude.lst"

static void setup(void **state) {
    CSYNC *csync;
    int rc;

    rc = csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2");
    assert_int_equal(rc, 0);

    *state = csync;
}

static void setup_init(void **state) {
    CSYNC *csync;
    int rc;

    rc = csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2");
    assert_int_equal(rc, 0);

    rc = csync_exclude_load(EXCLUDE_LIST_FILE, &(csync->excludes));
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

    *state = NULL;
}

static void check_csync_exclude_add(void **state)
{
  CSYNC *csync = *state;
  _csync_exclude_add(&(csync->excludes), "/tmp/check_csync1/*");
  assert_string_equal(csync->excludes->vector[0], "/tmp/check_csync1/*");
}

static void check_csync_exclude_load(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_exclude_load(EXCLUDE_LIST_FILE, &(csync->excludes) );
    assert_int_equal(rc, 0);

    assert_string_equal(csync->excludes->vector[0], "*.filepart");
    assert_int_not_equal(csync->excludes->count, 0);
}

static void check_csync_excluded(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_excluded(csync, "krawel_krawel", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded(csync, ".kde/share/config/kwin.eventsrc", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded(csync, ".htaccess/cache-maximegalon/cache1.txt", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded(csync, "mozilla/.htaccess", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /*
     * Test for patterns in subdirs. '.beagle' is defined as a pattern and has
     * to be found in top dir as well as in directories underneath.
     */
    rc = csync_excluded(csync, ".apdisk", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded(csync, "foo/.apdisk", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded(csync, "foo/bar/.apdisk", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded(csync, ".java", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    /* Files in the ignored dir .java will also be ignored. */
    rc = csync_excluded(csync, ".apdisk/totally_amazing.jar", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /* and also in subdirs */
    rc = csync_excluded(csync, "projects/.apdisk/totally_amazing.jar", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /* csync-journal is ignored in general silently. */
    rc = csync_excluded(csync, ".csync_journal.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded(csync, ".csync_journal.db.ctmp", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded(csync, "subdir/.csync_journal.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);

    /* pattern ]*.directory - ignore and remove */
    rc = csync_excluded(csync, "my.~directory", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_AND_REMOVE);

    rc = csync_excluded(csync, "/a_folder/my.~directory", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_AND_REMOVE);

    /* Not excluded because the pattern .netscape/cache requires directory. */
    rc = csync_excluded(csync, ".netscape/cache", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

}

static void check_csync_pathes(void **state)
{
    CSYNC *csync = *state;
    int rc;

    _csync_exclude_add( &(csync->excludes), "/exclude" );

    /* Check toplevel dir, the pattern only works for toplevel dir. */
    rc = csync_excluded(csync, "/exclude", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded(csync, "/foo/exclude", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    /* check for a file called exclude. Must still work */
    rc = csync_excluded(csync, "/exclude", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded(csync, "/foo/exclude", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    /* Add an exclude for directories only: excl/ */
    _csync_exclude_add( &(csync->excludes), "excl/" );
    rc = csync_excluded(csync, "/excl", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded(csync, "meep/excl", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded(csync, "/excl", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
}

static void check_csync_is_windows_reserved_word() {
    assert_true(csync_is_windows_reserved_word("CON"));
    assert_true(csync_is_windows_reserved_word("con"));
    assert_true(csync_is_windows_reserved_word("CON."));
    assert_true(csync_is_windows_reserved_word("con."));
    assert_true(csync_is_windows_reserved_word("CON.ference"));
    assert_false(csync_is_windows_reserved_word("CONference"));
    assert_false(csync_is_windows_reserved_word("conference"));
    assert_false(csync_is_windows_reserved_word("conf.erence"));
    assert_false(csync_is_windows_reserved_word("co"));
    assert_true(csync_is_windows_reserved_word("A:"));
    assert_true(csync_is_windows_reserved_word("a:"));
    assert_true(csync_is_windows_reserved_word("z:"));
    assert_true(csync_is_windows_reserved_word("Z:"));
    assert_true(csync_is_windows_reserved_word("M:"));
    assert_true(csync_is_windows_reserved_word("m:"));

}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_csync_exclude_add, setup, teardown),
        unit_test_setup_teardown(check_csync_exclude_load, setup, teardown),
        unit_test_setup_teardown(check_csync_excluded, setup_init, teardown),
        unit_test_setup_teardown(check_csync_pathes, setup_init, teardown),
        unit_test_setup_teardown(check_csync_is_windows_reserved_word, setup_init, teardown),
    };

    return run_tests(tests);
}
