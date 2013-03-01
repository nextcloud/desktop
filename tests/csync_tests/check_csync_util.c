#include "torture.h"

#include "csync_util.h"

static void check_csync_instruction_str(void **state)
{
  const char *str;

  (void) state; /* unused */

  str = csync_instruction_str(CSYNC_INSTRUCTION_ERROR);
  assert_string_equal(str, "INSTRUCTION_ERROR");

  str = csync_instruction_str(0xFFFF);
  assert_string_equal(str, "ERROR!");
}

static void check_csync_memstat(void **state)
{
  (void) state; /* unused */

  csync_memstat_check();
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test(check_csync_instruction_str),
        unit_test(check_csync_memstat),
    };

    return run_tests(tests);
}

