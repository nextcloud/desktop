#include <stdlib.h>
#include <time.h>

#include "torture.h"

#include "std/c_list.h"

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

static void setup_complete_list(void **state) {
    c_list_t *list = NULL;
    int i = 0;
    srand(1);

    for (i = 0; i < 100; i++) {
        test_t *testdata = NULL;

        testdata = malloc(sizeof(test_t));
        assert_non_null(testdata);

        testdata->key = i;
        testdata->number = rand() % 100;

        list = c_list_append(list, (void *) testdata);
        assert_non_null(list);
    }

    *state = list;
}

static void setup_random_list(void **state) {
    c_list_t *list = NULL;
    int i = 0;
    srand(1);

    for (i = 0; i < 100; i++) {
        test_t *testdata;

        testdata = malloc(sizeof(test_t));
        assert_non_null(testdata);

        testdata->key = i;
        testdata->number = rand() % 100;

        /* random insert */
        if (testdata->number > 49) {
            list = c_list_prepend(list, (void *) testdata);
        } else {
            list = c_list_append(list, (void *) testdata);
        }

        assert_non_null(list);
    }

    *state = list;
}

static void teardown_destroy_list(void **state) {
    c_list_t *list = *state;
    c_list_t *walk = NULL;

    for (walk = c_list_first(list); walk != NULL; walk = c_list_next(walk)) {
        test_t *data;

        data = (test_t *) walk->data;
        free(data);
    }
    c_list_free(list);
    *state = NULL;
}

/*
 * Start tests
 */
static void check_c_list_alloc(void **state)
{
    c_list_t *new = NULL;

    (void) state; /* unused */

    new = c_list_alloc();
    assert_non_null(new);

    free(new);
}

static void check_c_list_remove_null(void **state)
{
    (void) state; /* unused */

    assert_null(c_list_remove(NULL, NULL));
}

static void check_c_list_append(void **state)
{
    c_list_t *list = *state;

    list = c_list_append(list, (void *) 5);
    assert_non_null(list);

    list = c_list_remove(list, (void *) 5);
    assert_null(list);
}

static void check_c_list_prepend(void **state)
{
    c_list_t *list = *state;

    list = c_list_prepend(list, (void *) 5);
    assert_non_null(list);

    list = c_list_remove(list, (void *) 5);
    assert_null(list);
}

static void check_c_list_first(void **state)
{
    c_list_t *list = *state;

    c_list_t *first = NULL;
    test_t *data = NULL;

    first = c_list_first(list);
    assert_non_null(first);

    data = first->data;
    assert_int_equal(data->key, 0);
}

static void check_c_list_last(void **state)
{
    c_list_t *list = *state;
    c_list_t *last = NULL;
    test_t *data = NULL;

    last = c_list_last(list);
    assert_non_null(list);

    data = last->data;
    assert_int_equal(data->key, 99);
}

static void check_c_list_next(void **state)
{
    c_list_t *list = *state;
    c_list_t *first = NULL;
    c_list_t *next = NULL;
    test_t *data = NULL;

    first = c_list_first(list);
    assert_non_null(first);
    next = c_list_next(first);
    assert_non_null(next);

    data = next->data;
    assert_int_equal(data->key, 1);
}

static void check_c_list_prev(void **state)
{
    c_list_t *list = *state;
    c_list_t *last = NULL;
    c_list_t *prev = NULL;
    test_t *data = NULL;

    last = c_list_last(list);
    assert_non_null(last);
    prev = c_list_prev(last);
    assert_non_null(prev);

    data = prev->data;
    assert_int_equal(data->key, 98);
}

static void check_c_list_length(void **state)
{
    c_list_t *list = *state;
    unsigned long len = 0;

    len = c_list_length(list);
    assert_int_equal(len, 100);
}

static void check_c_list_position(void **state)
{
    c_list_t *list = *state;
    c_list_t *pos = NULL;
    test_t *data = NULL;

    pos = c_list_position(list, 50);
    assert_non_null(pos);

    data = pos->data;
    assert_int_equal(data->key, 50);
}

static void check_c_list_insert(void **state)
{
    c_list_t *list = *state;
    c_list_t *pos = NULL;
    test_t *data = NULL;

    data = malloc(sizeof(test_t));
    assert_non_null(data);

    data->key = data->number = 123;

    list = c_list_insert(list, (void *) data, 50);
    data = NULL;

    pos = c_list_position(list, 50);
    assert_non_null(pos);

    data = pos->data;
    assert_int_equal(data->key, 123);
}

static void check_c_list_find(void **state)
{
    c_list_t *list = *state;
    c_list_t *find = NULL;
    test_t *data = NULL;

    data = malloc(sizeof(test_t));
    assert_non_null(data);

    data->key = data->number = 123;

    list = c_list_insert(list, (void *) data, 50);

    find = c_list_find(list, data);
    assert_memory_equal(data, find->data, sizeof(test_t));
}

static void check_c_list_find_custom(void **state)
{
    c_list_t *list = *state;
    c_list_t *find = NULL;
    test_t *data = NULL;

    data = malloc(sizeof(test_t));
    assert_non_null(data);

    data->key = data->number = 123;

    list = c_list_insert(list, (void *) data, 50);

    find = c_list_find_custom(list, data, list_cmp);
    assert_memory_equal(data, find->data, sizeof(test_t));
}

static void check_c_list_insert_sorted(void **state)
{
    c_list_t *list = *state;
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
        assert_int_equal(data->key, i);
        i++;
    }

    c_list_free(new);
}

static void check_c_list_sort(void **state)
{
    c_list_t *list = *state;
    int i = 0;
    test_t *data = NULL;
    c_list_t *walk = NULL;

    /* sort list */
    list = c_list_sort(list, list_cmp);
    assert_non_null(list);

    /* check the list */
    for (walk = c_list_first(list); walk != NULL; walk = c_list_next(walk)) {
        data = (test_t *) walk->data;
        assert_int_equal(data->key, i);
        i++;
    }
}

int torture_run_tests(void)
{
  const UnitTest tests[] = {
      unit_test(check_c_list_alloc),
      unit_test(check_c_list_remove_null),
      unit_test(check_c_list_append),
      unit_test(check_c_list_prepend),
      unit_test_setup_teardown(check_c_list_first, setup_complete_list, teardown_destroy_list),
      unit_test_setup_teardown(check_c_list_last, setup_complete_list, teardown_destroy_list),
      unit_test_setup_teardown(check_c_list_next, setup_complete_list, teardown_destroy_list),
      unit_test_setup_teardown(check_c_list_prev, setup_complete_list, teardown_destroy_list),
      unit_test_setup_teardown(check_c_list_length, setup_complete_list, teardown_destroy_list),
      unit_test_setup_teardown(check_c_list_position, setup_complete_list, teardown_destroy_list),
      unit_test_setup_teardown(check_c_list_find, setup_complete_list, teardown_destroy_list),
      unit_test_setup_teardown(check_c_list_find_custom, setup_complete_list, teardown_destroy_list),
      unit_test_setup_teardown(check_c_list_insert, setup_complete_list, teardown_destroy_list),
      unit_test_setup_teardown(check_c_list_insert_sorted, setup_random_list, teardown_destroy_list),
      unit_test_setup_teardown(check_c_list_sort, setup_random_list, teardown_destroy_list),
  };

  return run_tests(tests);
}

