#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "support.h"

#include "std/c_dir.h"

const char *check_dir = "/tmp/check/c_mkdirs//with/check//";
const char *check_file = "/tmp/check/c_mkdirs/with/check/foobar.txt";

static void setup(void) {
  c_mkdirs(check_dir, 0755);
  system("touch /tmp/check/c_mkdirs/with/check/foobar.txt");
}

static void teardown(void) {
  system("rm -rf /tmp/check");
}

static int test_dir(const char *path, mode_t mode) {
  struct stat sb;
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

START_TEST (check_c_mkdirs_new)
{
  fail_unless(c_mkdirs(check_dir, 0755) == 0, NULL);
  fail_unless(test_dir(check_dir, 0755) == 0, NULL);
  system("rm -rf /tmp/check");
}
END_TEST

START_TEST (check_c_mkdirs_mode)
{
  fail_unless(c_mkdirs(check_dir, 0700) == 0, NULL);
  fail_unless(test_dir(check_dir, 0700) == 0, NULL);
  system("rm -rf /tmp/check");
}
END_TEST

START_TEST (check_c_mkdirs_existing_path)
{
  fail_unless(c_mkdirs(check_dir, 0755) == 0, NULL);
}
END_TEST

START_TEST (check_c_mkdirs_file)
{
  fail_unless(c_mkdirs(check_file, 0755) == -1 && errno == ENOTDIR, NULL);
}
END_TEST

START_TEST (check_c_mkdirs_null)
{
  fail_unless(c_mkdirs(NULL, 0755) == -1, NULL);
}
END_TEST

START_TEST (check_c_isdir)
{
  fail_unless(c_isdir(check_dir), NULL);
}
END_TEST

START_TEST (check_c_isdir_on_file)
{
  fail_unless(! c_isdir(check_file), NULL);
}
END_TEST

START_TEST (check_c_isdir_null)
{
  fail_unless(! c_isdir(NULL), NULL);
}
END_TEST

static Suite *make_std_c_mkdirs_suite(void) {
  Suite *s = suite_create("std:dir:c_mkdirs");

  create_case(s, "check_c_mkdirs_new", check_c_mkdirs_new);
  create_case(s, "check_c_mkdirs_mode", check_c_mkdirs_mode);
  create_case_fixture(s, "check_c_mkdirs_existing_path", check_c_mkdirs_existing_path, setup, teardown);
  create_case_fixture(s, "check_c_mkdirs_file", check_c_mkdirs_file, setup, teardown);
  create_case(s, "check_c_mkdirs_null", check_c_mkdirs_null);

  return s;
}

static Suite *make_std_c_isdir_suite(void) {
  Suite *s = suite_create("std:dir:c_isdir");

  create_case_fixture(s, "check_c_isdir", check_c_isdir, setup, teardown);
  create_case_fixture(s, "check_c_isdir_on_file", check_c_isdir_on_file, setup, teardown);
  create_case(s, "check_c_isdir_null", check_c_isdir_null);

  return s;
}

int main(void) {
  int nf;

  Suite *s = make_std_c_mkdirs_suite();
  Suite *s2 = make_std_c_isdir_suite();

  SRunner *sr;
  sr = srunner_create(s);
  srunner_add_suite (sr, s2);
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

