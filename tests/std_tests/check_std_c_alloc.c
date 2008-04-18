#include "support.h"

#include <string.h>

#include "std/c_alloc.h"

struct test_s {
  int answer;
};

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
  char *str = (char *) "test";
  char *tdup;

  tdup = c_strdup(str);
  fail_unless(strcmp(tdup, str) == 0, NULL);
  free(tdup);
}
END_TEST

static Suite *make_c_malloc_suite(void) {
  Suite *s = suite_create("std:alloc:malloc");

  create_case(s, "check_c_malloc", check_c_malloc);
  create_case(s, "check_c_malloc_zero", check_c_malloc_zero);

  return s;
}

static Suite *make_c_strdup_suite(void) {
  Suite *s = suite_create("std:alloc:strdup");

  create_case(s, "check_c_strdup", check_c_strdup);

  return s;
}

int main(void) {
  int nf;

  Suite *s = make_c_malloc_suite();
  Suite *s2 = make_c_strdup_suite();

  SRunner *sr;
  sr = srunner_create(s);
  srunner_add_suite (sr, s2);
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

