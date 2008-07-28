#include "support.h"

#include <stdlib.h>
#include <string.h>

#include "std/c_alloc.h"

struct test_s {
  int answer;
};

#ifdef CSYNC_MEM_NULL_TESTS
static void setup(void) {
  setenv("CSYNC_NOMEMORY", "1", 1);
}

static void teardown(void) {
  unsetenv("CSYNC_NOMEMORY");
}
#endif /* CSYNC_MEM_NULL_TESTS */

START_TEST (check_c_malloc)
{
  struct test_s *p = NULL;
  p = c_malloc(sizeof(struct test_s));
  fail_unless(p != NULL, NULL);
  fail_unless(p->answer == 0, NULL);
  p->answer = 42;
  fail_unless(p->answer == 42, NULL);
  free(p);
}
END_TEST

START_TEST (check_c_malloc_zero)
{
  void *p;
  p = c_malloc((size_t) 0);
  fail_unless(p == NULL, NULL);
}
END_TEST

START_TEST (check_c_strdup)
{
  const char *str = "test";
  char *tdup = NULL;

  tdup = c_strdup(str);
  fail_unless(strcmp(tdup, str) == 0, NULL);

  free(tdup);
}
END_TEST

START_TEST (check_c_strndup)
{
  const char *str = "test";
  char *tdup = NULL;

  tdup = c_strndup(str, 3);
  fail_unless(strncmp(tdup, "tes", 3) == 0, NULL);

  free(tdup);
}
END_TEST

#ifdef CSYNC_MEM_NULL_TESTS
START_TEST (check_c_malloc_nomem)
{
  struct test_s *p = NULL;

  p = c_malloc(sizeof(struct test_s));
  fail_unless(p == NULL, NULL);
}
END_TEST

START_TEST (check_c_strdup_nomem)
{
  const char *str = "test";
  char *tdup = NULL;

  tdup = c_strdup(str);
  fail_unless(tdup == NULL, NULL);
}
END_TEST

START_TEST (check_c_strndup_nomem)
{
  const char *str = "test";
  char *tdup = NULL;

  tdup = c_strndup(str, 3);
  fail_unless(tdup == NULL, NULL);
}
END_TEST
#endif /* CSYNC_MEM_NULL_TESTS */

static Suite *make_c_malloc_suite(void) {
  Suite *s = suite_create("std:alloc:malloc");

  create_case(s, "check_c_malloc", check_c_malloc);
  create_case(s, "check_c_malloc_zero", check_c_malloc_zero);

#ifdef CSYNC_MEM_NULL_TESTS
  create_case_fixture(s, "check_c_malloc_nomem", check_c_malloc_nomem, setup, teardown);
#endif

  return s;
}

static Suite *make_c_strdup_suite(void) {
  Suite *s = suite_create("std:alloc:strdup");

  create_case(s, "check_c_strdup", check_c_strdup);
  create_case(s, "check_c_strndup", check_c_strndup);

#ifdef CSYNC_MEM_NULL_TESTS
  create_case_fixture(s, "check_c_strdup_nomem", check_c_strdup_nomem, setup, teardown);
  create_case_fixture(s, "check_c_strndup_nomem", check_c_strndup_nomem, setup, teardown);
#endif

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

  s = make_c_malloc_suite();
  s2 = make_c_strdup_suite();

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

