#include "torture.h"

#include "std/c_alloc.h"

struct test_s {
  int answer;
};

static void check_c_malloc(void **state)
{
  struct test_s *p = NULL;

  (void) state; /* unused */

  p = c_malloc(sizeof(struct test_s));
  assert_non_null(p);
  assert_int_equal(p->answer, 0);
  p->answer = 42;
  assert_int_equal(p->answer, 42);
  free(p);
}

static void check_c_malloc_zero(void **state)
{
  void *p;

  (void) state; /* unused */

  p = c_malloc((size_t) 0);
  assert_null(p);
}

static void check_c_strdup(void **state)
{
  const char *str = "test";
  char *tdup = NULL;

  (void) state; /* unused */

  tdup = c_strdup(str);
  assert_string_equal(tdup, str);

  free(tdup);
}

static void check_c_strndup(void **state)
{
  const char *str = "test";
  char *tdup = NULL;

  (void) state; /* unused */

  tdup = c_strndup(str, 3);
  assert_memory_equal(tdup, "tes", 3);

  free(tdup);
}

int torture_run_tests(void)
{
  const UnitTest tests[] = {
      unit_test(check_c_malloc),
      unit_test(check_c_malloc_zero),
      unit_test(check_c_strdup),
      unit_test(check_c_strndup),
  };

  return run_tests(tests);
}

