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
  fail_if(c_mkdirs(check_dir, 0755) < 0, "Setup failed");
  fail_if(system("touch /tmp/check/c_mkdirs/with/check/foobar.txt") < 0, "Setup failed");
}

static void teardown(void) {
  fail_if(system("rm -rf /tmp/check") < 0, "Teardown failed");
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
  fail_if(system("rm -rf /tmp/check") < 0, NULL);
}
END_TEST

START_TEST (check_c_mkdirs_mode)
{
  fail_unless(c_mkdirs(check_dir, 0700) == 0, NULL);
  fail_unless(test_dir(check_dir, 0700) == 0, NULL);
  fail_if(system("rm -rf /tmp/check") < 0, NULL);
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

int main(int argc, char **argv) {
  Suite *s = NULL;
  Suite *s2 = NULL;
  SRunner *sr = NULL;
  struct argument_s arguments;
  int nf;

  ZERO_STRUCT(arguments);

  cmdline_parse(argc, argv, &arguments);

  s = make_std_c_mkdirs_suite();
  s2 = make_std_c_isdir_suite();

  sr = srunner_create(s);
  if (arguments.nofork) {
    srunner_set_fork_status(sr, CK_NOFORK);
  }
  srunner_add_suite (sr, s2);
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

