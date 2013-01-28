#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "torture.h"

#include "std/c_private.h"
#include "std/c_file.h"

const char *check_dir = "/tmp/check";
const char *check_src_file = "/tmp/check/foo.txt";
const char *check_dst_file = "/tmp/check/bar.txt";

static int test_file(const char *path, mode_t mode) {
  struct stat sb;
  if (lstat(path, &sb) < 0) {
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
    assert_int_equal(errno, EISDIR);
}

int torture_run_tests(void)
{
  const UnitTest tests[] = {
      unit_test_setup_teardown(check_c_copy, setup, teardown),
      unit_test(check_c_copy_same_file),
      unit_test_setup_teardown(check_c_copy_isdir, setup, teardown),
  };

  return run_tests(tests);
}

