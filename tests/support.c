#include "support.h"

#include <stdio.h>

void create_case(Suite *s, const char *name, TFun function) {
  TCase *tc_new = tcase_create(name);
  tcase_set_timeout(tc_new, 30);
  suite_add_tcase (s, tc_new);
  tcase_add_test(tc_new, function);
}

void create_case_fixture(Suite *s, const char *name, TFun function, void (*setup)(void), void (*teardown)(void)) {
  TCase *tc_new = tcase_create(name);
  tcase_add_checked_fixture(tc_new, setup, teardown);
  tcase_set_timeout(tc_new, 30);
  suite_add_tcase (s, tc_new);
  tcase_add_test(tc_new, function);
}

void create_case_timeout(Suite *s, const char *name, TFun function, int timeout) {
  TCase *tc_new = tcase_create(name);
  tcase_set_timeout(tc_new, timeout);
  suite_add_tcase (s, tc_new);
  tcase_add_test(tc_new, function);
}

