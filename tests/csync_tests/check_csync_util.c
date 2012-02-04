#include "support.h"

#include "csync_util.h"

START_TEST (check_csync_instruction_str)
{
  const char *str = NULL;

  str = csync_instruction_str(CSYNC_INSTRUCTION_ERROR);
  fail_unless(strcmp(str, "INSTRUCTION_ERROR") == 0, NULL);

  str = csync_instruction_str(0xFFFF);
  fail_unless(strcmp(str, "ERROR!") == 0, NULL);
}
END_TEST

START_TEST (check_csync_memstat)
{
  csync_memstat_check();
}
END_TEST


static Suite *make_csync_suite(void) {
  Suite *s = suite_create("csync_lock");

  create_case(s, "check_csync_instruction_str", check_csync_instruction_str);
  create_case(s, "check_csync_memstat", check_csync_memstat);

  return s;
}

int main(int argc, char **argv) {
  Suite *s = NULL;
  SRunner *sr = NULL;
  struct argument_s arguments;
  int nf;

  ZERO_STRUCT(arguments);

  cmdline_parse(argc, argv, &arguments);

  s = make_csync_suite();

  sr = srunner_create(s);
  if (arguments.nofork) {
    srunner_set_fork_status(sr, CK_NOFORK);
  }
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

