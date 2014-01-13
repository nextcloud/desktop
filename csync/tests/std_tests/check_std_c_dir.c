#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "torture.h"

#include "std/c_private.h"
#include "std/c_dir.h"
#include "std/c_string.h"

const char *check_dir = "/tmp/check/c_mkdirs//with/check//";
const char *check_file = "/tmp/check/c_mkdirs/with/check/foobar.txt";

static void setup(void **state) {
    int rc;

    (void) state; /* unused */

    rc = c_mkdirs(check_dir, 0755);
    assert_int_equal(rc, 0);
    rc = system("touch /tmp/check/c_mkdirs/with/check/foobar.txt");
    assert_int_equal(rc, 0);
}

static void teardown(void **state) {
    int rc;

    (void) state; /* unused */

    rc = c_rmdirs(check_dir);
    assert_int_equal(rc, 0);
}

static int test_dir(const char *path, mode_t mode) {
  csync_stat_t sb;
  if (lstat(path, &sb) < 0) {
    return -1;
  }

  if (! S_ISDIR(sb.st_mode)) {
    return -1;
  }

  /* FIXME */
  if ((sb.st_mode & mode) == mode) {
    return 0;
  }

  return -1;
}

static void check_c_mkdirs_rmdirs(void **state)
{
    csync_stat_t sb;
    int rc;
    mbchar_t *wcheck_dir;

    (void) state; /* unused */

    rc = c_mkdirs(check_dir, 0755);
    assert_int_equal(rc, 0);
    rc = test_dir(check_dir, 0755);
    assert_int_equal(rc, 0);
    rc = c_rmdirs(check_dir);
    assert_int_equal(rc, 0);
    wcheck_dir = c_utf8_to_locale(check_dir);
    rc = _tstat(wcheck_dir, &sb);
    c_free_locale_string(wcheck_dir);
    assert_int_equal(rc, -1);
}

static void check_c_mkdirs_mode(void **state)
{
    csync_stat_t sb;
    int rc;
    mbchar_t *wcheck_dir;

    (void) state; /* unused */
    rc = c_mkdirs(check_dir, 0700);
    assert_int_equal(rc, 0);
    rc = test_dir(check_dir, 0700);
    assert_int_equal(rc, 0);
    rc = c_rmdirs(check_dir);
    assert_int_equal(rc, 0);
    wcheck_dir = c_utf8_to_locale(check_dir);
    rc = _tstat(wcheck_dir, &sb);
    assert_int_equal(rc, -1);
    c_free_locale_string(wcheck_dir);
}

static void check_c_mkdirs_existing_path(void **state)
{
    int rc;

    (void) state; /* unused */

    rc = c_mkdirs(check_dir, 0755);
    assert_int_equal(rc, 0);
}

static void check_c_mkdirs_file(void **state)
{
    int rc;

    (void) state; /* unused */

    rc = c_mkdirs(check_file, 0755);
    assert_int_equal(rc, -1);
    assert_int_equal(errno, ENOTDIR);
}

static void check_c_mkdirs_null(void **state)
{
    (void) state; /* unused */

    assert_int_equal(c_mkdirs(NULL, 0755), -1);
}

static void check_c_isdir(void **state)
{
    (void) state; /* unused */

    assert_int_equal(c_isdir(check_dir), 1);
}

static void check_c_isdir_on_file(void **state)
{
    (void) state; /* unused */

    assert_int_equal(c_isdir(check_file), 0);
}

static void check_c_isdir_null(void **state)
{
    (void) state; /* unused */

    assert_int_equal(c_isdir(NULL), 0);
}

int torture_run_tests(void)
{
  const UnitTest tests[] = {
      unit_test(check_c_mkdirs_rmdirs),
      unit_test(check_c_mkdirs_mode),
      unit_test_setup_teardown(check_c_mkdirs_existing_path, setup, teardown),
      unit_test_setup_teardown(check_c_mkdirs_file, setup, teardown),
      unit_test(check_c_mkdirs_null),
      unit_test_setup_teardown(check_c_isdir, setup, teardown),
      unit_test_setup_teardown(check_c_isdir_on_file, setup, teardown),
      unit_test(check_c_isdir_null),
  };

  return run_tests(tests);
}

