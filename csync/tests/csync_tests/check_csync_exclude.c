#include "config.h"
#include <string.h>

#include "torture.h"

#define CSYNC_TEST 1
#include "csync_exclude.c"

static void setup(void **state) {
    CSYNC *csync;
    int rc;

    rc = csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2");
    assert_int_equal(rc, 0);

    free(csync->options.config_dir);
    csync->options.config_dir = c_strdup("/tmp/check_csync1/");
    assert_non_null(csync->options.config_dir);

    *state = csync;
}

static void setup_init(void **state) {
    CSYNC *csync;
    int rc;

    rc = csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2");
    assert_int_equal(rc, 0);

    free(csync->options.config_dir);
    csync->options.config_dir = c_strdup("/tmp/check_csync1/");
    assert_non_null(csync->options.config_dir);

    rc = csync_exclude_load(csync, BINARYDIR "/config/" CSYNC_EXCLUDE_FILE);
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
  _csync_exclude_add(csync, (const char *) "/tmp/check_csync1/*");
  assert_string_equal(csync->excludes->vector[0], "/tmp/check_csync1/*");
}

static void check_csync_exclude_load(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_exclude_load(csync, BINARYDIR "/config/" CSYNC_EXCLUDE_FILE);
    assert_int_equal(rc, 0);

    assert_string_equal(csync->excludes->vector[0], ".beagle");
}

static void check_csync_excluded(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_excluded(csync, "krawel_krawel", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded(csync, ".kde/share/config/kwin.eventsrc", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded(csync, ".kde4/cache-maximegalon/cache1.txt", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded(csync, ".mozilla/plugins", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /*
     * Test for patterns in subdirs. '.beagle' is defined as a pattern and has
     * to be found in top dir as well as in directories underneath.
     */
    rc = csync_excluded(csync, ".beagle", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded(csync, "foo/.beagle", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded(csync, "foo/bar/.beagle", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /*
     * Pattern: .java/
     * A file wont be excluded but a directory .java will be.
     */
    rc = csync_excluded(csync, ".java", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded(csync, ".java", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    /* Files in the ignored dir .java will also be ignored. */
    rc = csync_excluded(csync, ".java/totally_amazing.jar", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /* and also in subdirs */
    rc = csync_excluded(csync, "projects/.java/totally_amazing.jar", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /* csync-journal is ignored in general silently. */
    rc = csync_excluded(csync, ".csync_journal.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded(csync, ".csync_journal.db.ctmp", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded(csync, "subdir/.csync_journal.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);

    /* pattern ]*.directory - ignore and remove */
    rc = csync_excluded(csync, "my.directory", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_AND_REMOVE);

    rc = csync_excluded(csync, "/a_folder/my.directory", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_AND_REMOVE);

    /* Not excluded because the pattern .netscape/cache requires directory. */
    rc = csync_excluded(csync, ".netscape/cache", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    rc = csync_excluded(csync, ".netscape/cache", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /* Excluded because the parent dir .netscape/cache is ingored. */
    rc = csync_excluded(csync, ".netscape/cache/foo", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded(csync, ".netscape/cache/bar.txt", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded(csync, ".netscape/cache/longrun", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);


}


int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_csync_exclude_add, setup, teardown),
        unit_test_setup_teardown(check_csync_exclude_load, setup, teardown),
        unit_test_setup_teardown(check_csync_excluded, setup_init, teardown),
    };

    return run_tests(tests);
}
