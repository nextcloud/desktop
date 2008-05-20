#include <string.h>
#include <time.h>

#include "support.h"

#include "std/c_list.h"

static c_list_t *list = NULL;

typedef struct test_s {
  int key;
  int number;
} test_t;

/* compare function for sorting */
static int list_cmp(const void *key, const void *data) {
  test_t *a, *b;

  a = (test_t *) key;
  b = (test_t *) data;

  if (a->key < b->key) {
    return -1;
  } else if (a->key > b->key) {
    return 1;
  }

  return 0;
}

static void setup_complete_list(void) {
  int i = 0;
  srand(1);

  for (i = 0; i < 100; i++) {
    test_t *testdata = NULL;

    testdata = c_malloc(sizeof(test_t));
    fail_if(testdata == NULL, NULL);

    testdata->key = i;
    testdata->number = rand() % 100;

    list = c_list_append(list, (void *) testdata);

    fail_if(list == NULL, "Setup failed");
  }
}

static void setup_random_list(void) {
  int i = 0;
  srand(1);

  for (i = 0; i < 100; i++) {
    test_t *testdata = NULL;

    testdata = c_malloc(sizeof(test_t));
    fail_if(testdata == NULL, NULL);

    testdata->key = i;
    testdata->number = rand() % 100;

    /* random insert */
    if (testdata->number > 49) {
      list = c_list_prepend(list, (void *) testdata);
    } else {
      list = c_list_append(list, (void *) testdata);
    }

    fail_if(list == NULL, "Setup failed");
  }
}

static void teardown_destroy_list(void) {
  c_list_t *walk = NULL;

  for (walk = c_list_first(list); walk != NULL; walk = c_list_next(walk)) {
    test_t *data = NULL;

    data = (test_t *) walk->data;
    SAFE_FREE(data);
  }
  c_list_free(list);
  list = NULL;
}

/*
 * Start tests
 */
START_TEST (check_c_list_alloc)
{
  c_list_t *new = NULL;
  new = c_list_alloc();
  fail_if(new == NULL, NULL);

  SAFE_FREE(new);
}
END_TEST

START_TEST (check_c_list_remove_null)
{
  fail_unless(c_list_remove(NULL, NULL) == NULL, NULL);
}
END_TEST

START_TEST (check_c_list_append)
{
  list = c_list_append(list, (void *) 5);
  fail_if(list == NULL, NULL);

  list = c_list_remove(list, (void *) 5);
  fail_unless(list == NULL, NULL);
}
END_TEST

START_TEST (check_c_list_prepend)
{
  list = c_list_prepend(list, (void *) 5);
  fail_if(list == NULL, NULL);

  list = c_list_remove(list, (void *) 5);
  fail_unless(list == NULL, NULL);
}
END_TEST

START_TEST (check_c_list_first)
{
  c_list_t *first = NULL;
  test_t *data = NULL;

  first = c_list_first(list);
  fail_if(first == NULL);

  data = first->data;
  fail_unless(data->key == 0, NULL);
}
END_TEST

START_TEST (check_c_list_last)
{
  c_list_t *last = NULL;
  test_t *data = NULL;

  last = c_list_last(list);
  fail_if(last == NULL);

  data = last->data;
  fail_unless(data->key == 99, NULL);
}
END_TEST

START_TEST (check_c_list_next)
{
  c_list_t *first = NULL;
  c_list_t *next = NULL;
  test_t *data = NULL;

  first = c_list_first(list);
  fail_if(first == NULL);
  next = c_list_next(first);
  fail_if(next == NULL);

  data = next->data;
  fail_unless(data->key == 1, NULL);
}
END_TEST

START_TEST (check_c_list_prev)
{
  c_list_t *last = NULL;
  c_list_t *prev = NULL;
  test_t *data = NULL;

  last = c_list_last(list);
  fail_if(last == NULL);
  prev = c_list_prev(last);
  fail_if(prev == NULL);

  data = prev->data;
  fail_unless(data->key == 98, NULL);
}
END_TEST

START_TEST (check_c_list_length)
{
  unsigned long len = 0;

  len = c_list_length(list);
  fail_unless(len == 100, "len = %lu", len);
}
END_TEST

START_TEST (check_c_list_position)
{
  c_list_t *pos = NULL;
  test_t *data = NULL;

  pos = c_list_position(list, 50);
  fail_if(pos == NULL, NULL);

  data = pos->data;
  fail_unless(data->key == 50, "key: %d", data->key);
}
END_TEST

START_TEST (check_c_list_insert)
{
  c_list_t *pos = NULL;
  test_t *data = NULL;

  data = c_malloc(sizeof(test_t));
  fail_if(data == NULL, NULL);

  data->key = data->number = 123;

  list = c_list_insert(list, (void *) data, 50);
  data = NULL;

  pos = c_list_position(list, 50);
  fail_if(pos == NULL, NULL);

  data = pos->data;
  fail_unless(data->key == 123, "key: %d", data->key);
}
END_TEST

START_TEST (check_c_list_find)
{
  c_list_t *find = NULL;
  test_t *data = NULL;

  data = c_malloc(sizeof(test_t));
  fail_if(data == NULL, NULL);

  data->key = data->number = 123;

  list = c_list_insert(list, (void *) data, 50);

  find = c_list_find(list, data);
  fail_unless(data == (test_t *) find->data, NULL);
}
END_TEST

START_TEST (check_c_list_find_custom)
{
  c_list_t *find = NULL;
  test_t *data = NULL;

  data = c_malloc(sizeof(test_t));
  fail_if(data == NULL, NULL);

  data->key = data->number = 123;

  list = c_list_insert(list, (void *) data, 50);

  find = c_list_find_custom(list, data, list_cmp);
  fail_unless(data == (test_t *) find->data, NULL);
}
END_TEST

START_TEST (check_c_list_insert_sorted)
{
  int i = 0;
  c_list_t *walk = NULL;
  c_list_t *new = NULL;
  test_t *data = NULL;

  /* create the list */
  for (walk = c_list_first(list); walk != NULL; walk = c_list_next(walk)) {
    new = c_list_insert_sorted(new, walk->data, list_cmp);
  }

  /* check the list */
  for (walk = c_list_first(new); walk != NULL; walk = c_list_next(walk)) {
    data = (test_t *) walk->data;
    fail_unless(data->key == i, "key: %d, i: %d", data->key, i);
    i++;
  }

  c_list_free(new);
}
END_TEST

START_TEST (check_c_list_sort)
{
  int i = 0;
  test_t *data = NULL;
  c_list_t *walk = NULL;

  /* sort list */
  list = c_list_sort(list, list_cmp);
  fail_if(list == NULL, NULL);

  /* check the list */
  for (walk = c_list_first(list); walk != NULL; walk = c_list_next(walk)) {
    data = (test_t *) walk->data;
    fail_unless(data->key == i, "key: %d, i: %d", data->key, i);
    i++;
  }
}
END_TEST

#if 0
START_TEST (check_c_list_x)
{
}
END_TEST
#endif

static Suite *make_std_c_suite(void) {
  Suite *s = suite_create("std:path:c_basename");

  create_case(s, "check_c_list_alloc", check_c_list_alloc);
  create_case(s, "check_c_list_remove_null", check_c_list_remove_null);
  create_case(s, "check_c_list_append", check_c_list_append);
  create_case(s, "check_c_list_prepend", check_c_list_prepend);
  create_case_fixture(s, "check_c_list_first", check_c_list_first, setup_complete_list, teardown_destroy_list);
  create_case_fixture(s, "check_c_list_last", check_c_list_last, setup_complete_list, teardown_destroy_list);
  create_case_fixture(s, "check_c_list_next", check_c_list_next, setup_complete_list, teardown_destroy_list);
  create_case_fixture(s, "check_c_list_prev", check_c_list_prev, setup_complete_list, teardown_destroy_list);
  create_case_fixture(s, "check_c_list_length", check_c_list_length, setup_complete_list, teardown_destroy_list);
  create_case_fixture(s, "check_c_list_position", check_c_list_position, setup_complete_list, teardown_destroy_list);
  create_case_fixture(s, "check_c_list_find", check_c_list_find, setup_complete_list, teardown_destroy_list);
  create_case_fixture(s, "check_c_list_find_custom", check_c_list_find_custom, setup_complete_list, teardown_destroy_list);
  create_case_fixture(s, "check_c_list_insert", check_c_list_insert, setup_complete_list, teardown_destroy_list);
  create_case_fixture(s, "check_c_list_insert_sorted", check_c_list_insert_sorted, setup_random_list, teardown_destroy_list);
  create_case_fixture(s, "check_c_list_sort", check_c_list_sort, setup_random_list, teardown_destroy_list);

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

