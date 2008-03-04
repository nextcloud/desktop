#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "support.h"

#include "std/c_string.h"

START_TEST (check_c_streq_equal)
{
  fail_unless(c_streq("test", "test"), NULL);
}
END_TEST

START_TEST (check_c_streq_not_equal)
{
  fail_if(c_streq("test", "test2"), NULL);
}
END_TEST

START_TEST (check_c_streq_null)
{
  fail_if(c_streq(NULL, "test"), NULL);
  fail_if(c_streq("test", NULL), NULL);
  fail_if(c_streq(NULL, NULL), NULL);
}
END_TEST


static Suite *make_std_c_streq_suite(void) {
  Suite *s = suite_create("std:string:c_streq");

  create_case(s, "check_c_streq_equal", check_c_streq_equal);
  create_case(s, "check_c_streq_not_equal", check_c_streq_not_equal);
  create_case(s, "check_c_streq_null", check_c_streq_null);

  return s;
}

START_TEST (check_c_strlist_new)
{
  c_strlist_t *strlist = NULL;

  strlist = c_strlist_new(42);
  fail_if(strlist == NULL, NULL);
  fail_unless(strlist->size == 42, NULL);
  fail_unless(strlist->count == 0, NULL);

  c_strlist_destroy(strlist);
}
END_TEST

START_TEST (check_c_strlist_add)
{
  size_t i = 0;
  c_strlist_t *strlist = NULL;

  strlist = c_strlist_new(42);
  fail_if(strlist == NULL, NULL);
  fail_unless(strlist->size == 42, NULL);
  fail_unless(strlist->count == 0, NULL);

  for (i = 0; i < strlist->size; i++) {
    fail_unless(c_strlist_add(strlist, (char *) "foobar") == 0, NULL);
  }

  fail_unless(strlist->count == 42, NULL);
  fail_unless(strcmp(strlist->vector[0], "foobar") == 0, NULL);
  fail_unless(strcmp(strlist->vector[41], "foobar") == 0, NULL);

  c_strlist_destroy(strlist);
}
END_TEST

static Suite *make_std_c_strlist_suite(void) {
  Suite *s = suite_create("std:str:c_stringlist");

  create_case(s, "check_c_strlist_new", check_c_strlist_new);
  create_case(s, "check_c_strlist_add", check_c_strlist_add);

  return s;
}

int main(void) {
  int nf;

  Suite *s = make_std_c_streq_suite();
  Suite *s2 = make_std_c_strlist_suite();

  SRunner *sr;
  sr = srunner_create(s);
  srunner_add_suite (sr, s2);
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

