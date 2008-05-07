#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "support.h"

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

static void setup(void) {
  system("mkdir -p /tmp/check");
  system("echo 42 > /tmp/check/foo.txt");
}

static void teardown(void) {
  system("rm -rf /tmp/check");
}

START_TEST (check_c_copy)
{
  fail_unless(c_copy(check_src_file, check_dst_file, 0644) == 0, NULL);
  fail_unless(test_file(check_dst_file, 0644) == 0, NULL);
}
END_TEST

START_TEST (check_c_copy_same_file)
{
  fail_unless(c_copy(check_src_file, check_src_file, 0644) == -1, NULL);
}
END_TEST

START_TEST (check_c_copy_isdir)
{
  fail_unless((c_copy(check_src_file, check_dir, 0644) == -1) && (errno == EISDIR), NULL);
  fail_unless((c_copy(check_dir, check_dst_file, 0644) == -1) && (errno == EISDIR), NULL);
}
END_TEST

static Suite *make_c_copy_suite(void) {
  Suite *s = suite_create("std:file:c_copy");

  create_case_fixture(s, "check_c_copy", check_c_copy, setup, teardown);
  create_case(s, "check_c_copy_same_file", check_c_copy_same_file);
  create_case_fixture(s, "check_c_copy_isdir", check_c_copy_isdir, setup, teardown);

  return s;
}

int main(int argc, char **argv) {
  Suite *s = NULL;
  SRunner *sr = NULL;
  struct argument_s arguments;
  int nf;

  ZERO_STRUCT(arguments);

  cmdline_parse(argc, argv, &arguments);

  s = make_c_copy_suite();

  sr = srunner_create(s);
  if (arguments.nofork) {
    srunner_set_fork_status(sr, CK_NOFORK);
  }
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

