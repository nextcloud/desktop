#include <string.h>

#include "support.h"

#include "std/c_time.h"

START_TEST (check_c_tspecdiff)
{
  struct timespec start, finish, diff;

  clock_gettime(CLOCK_REALTIME, &start);
  clock_gettime(CLOCK_REALTIME, &finish);

  diff = c_tspecdiff(finish, start);

  fail_unless(diff.tv_sec == 0, NULL);
  fail_unless(diff.tv_nsec > 0, NULL);
}
END_TEST

START_TEST (check_c_secdiff)
{
  struct timespec start, finish;
  double diff;

  clock_gettime(CLOCK_REALTIME, &start);
  clock_gettime(CLOCK_REALTIME, &finish);

  diff = c_secdiff(finish, start);

  fail_unless(diff > 0.00 && diff < 1.00, NULL);
}
END_TEST

static Suite *make_std_c_suite(void) {
  Suite *s = suite_create("std:path:c_basename");

  create_case(s, "check_c_tspecdiff", check_c_tspecdiff);
  create_case(s, "check_c_secdiff", check_c_secdiff);

  return s;
}

int main(int argc, char **argv) {
  Suite *s = NULL;
  SRunner *sr = NULL;
  struct argument_s arguments;
  int nf;

  ZERO_STRUCT(arguments);

  cmdline_parse(argc, argv, &arguments);

  s = make_std_c_suite();

  sr = srunner_create(s);
  if (arguments.nofork) {
    srunner_set_fork_status(sr, CK_NOFORK);
  }
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

