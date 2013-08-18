#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "torture.h"

#include "std/c_private.h"
#include "std/c_file.h"
#include "std/c_string.h"

const char *check_dir = "/tmp/check";
const char *check_src_file = "/tmp/check/foo.txt";
const char *check_dst_file = "/tmp/check/bar.txt";

static int test_file(const char *path, mode_t mode) {
  csync_stat_t sb;
  mbchar_t *mbpath = c_utf8_to_locale(path);
  int rc = _tstat(mbpath, &sb);
  c_free_locale_string(mbpath);

  if (rc < 0) {
    return -1;
  }

  if (! S_ISREG(sb.st_mode)) {
    return -1;
  }

  if ((sb.st_mode & mode) == mode) {
    return 0;
  }

  return -1;
}

static void setup(void **state) {
    int rc;

    (void) state; /* unused */

    rc = system("mkdir -p /tmp/check");
    assert_int_equal(rc, 0);
    rc = system("echo 42 > /tmp/check/foo.txt");
    assert_int_equal(rc, 0);
}

static void teardown(void **state) {
    int rc;

    (void) state; /* unused */

    rc = system("rm -rf /tmp/check");
    assert_int_equal(rc, 0);
}

static void check_c_copy(void **state)
{
    int rc;

    (void) state; /* unused */

    rc = c_copy(check_src_file, check_dst_file, 0644);
    assert_int_equal(rc, 0);
    rc = test_file(check_dst_file, 0644);
    assert_int_equal(rc, 0);
}

static void check_c_copy_same_file(void **state)
{
    int rc;

    (void) state; /* unused */

    rc = c_copy(check_src_file, check_src_file, 0644);
    assert_int_equal(rc, -1);
}

static void check_c_copy_isdir(void **state)
{
    int rc;

    (void) state; /* unused */

    rc = c_copy(check_src_file, check_dir, 0644);
    assert_int_equal(rc, -1);
    assert_int_equal(errno, EISDIR);

    rc = c_copy(check_dir, check_dst_file, 0644);
    assert_int_equal(rc, -1);
    assert_int_equal(errno, ENOENT);
}

static void check_c_compare_file(void **state)
{
  int rc;
  (void) state;

  rc = c_copy(check_src_file, check_dst_file, 0644);
  assert_int_equal(rc, 0);

  rc = c_compare_file( check_src_file, check_dst_file );
  assert_int_equal(rc, 1);

  /* Check error conditions */
  rc = c_compare_file( NULL, check_dst_file );
  assert_int_equal(rc, -1);
  rc = c_compare_file( check_dst_file, NULL );
  assert_int_equal(rc, -1);
  rc = c_compare_file( NULL, NULL );
  assert_int_equal(rc, -1);

  rc = c_compare_file( check_src_file, "/I_do_not_exist_in_the_filesystem.dummy");
  assert_int_equal(rc, -1);
  rc = c_compare_file( "/I_do_not_exist_in_the_filesystem.dummy", check_dst_file);
  assert_int_equal(rc, -1);

  rc = system("echo \"hallo42\" > /tmp/check/foo.txt");
  assert_int_equal(rc, 0);
  rc = system("echo \"hallo52\" > /tmp/check/bar.txt");
  assert_int_equal(rc, 0);
  rc = c_compare_file( check_src_file, check_dst_file );
  assert_int_equal(rc, 0);

  /* Create two 1MB random files */
  rc = system("dd if=/dev/urandom of=/tmp/check/foo.txt bs=1024 count=1024");
  assert_int_equal(rc, 0);
  rc = system("dd if=/dev/urandom of=/tmp/check/bar.txt bs=1024 count=1024");
  assert_int_equal(rc, 0);
  rc = c_compare_file( check_src_file, check_dst_file );
  assert_int_equal(rc, 0);

  /* Create two 1MB random files with different size */
  rc = system("dd if=/dev/urandom of=/tmp/check/foo.txt bs=1024 count=1024");
  assert_int_equal(rc, 0);
  rc = system("dd if=/dev/urandom of=/tmp/check/bar.txt bs=1024 count=1020");
  assert_int_equal(rc, 0);
  rc = c_compare_file( check_src_file, check_dst_file );
  assert_int_equal(rc, 0);

  /* compare two big files which are equal */
  rc = c_copy(check_src_file, check_dst_file, 0644);
  assert_int_equal(rc, 0);

  rc = c_compare_file( check_src_file, check_dst_file );
  assert_int_equal(rc, 1);
}

int torture_run_tests(void)
{
  const UnitTest tests[] = {
      unit_test_setup_teardown(check_c_copy, setup, teardown),
      unit_test(check_c_copy_same_file),
      unit_test_setup_teardown(check_c_copy_isdir, setup, teardown),
      unit_test_setup_teardown(check_c_compare_file, setup, teardown),
  };

  return run_tests(tests);
}

