#include <errno.h>
#include <time.h>
#include "support.h"

#include "std/c_alloc.h"
#include "std/c_rbtree.h"

typedef struct test_s {
  int key;
  int number;
} test_t;

static c_rbtree_t *tree = NULL;

static int data_cmp(const void *key, const void *data) {
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

static int key_cmp(const void *key, const void *data) {
  int a;
  test_t *b;

  a = POINTER_TO_INT(key);
  b = (test_t *) data;

  if (a < b->key) {
    return -1;
  } else if (a > b->key) {
    return 1;
  }

  return 0;
}

static int visitor(void *obj, void *data) {
  test_t *a = NULL;
  test_t *b = NULL;

  a = (test_t *) obj;
  b = (test_t *) data;

  if (a->key == b->key) {
    a->number = 42;
  }

  return 0;
}

static void destructor(void *data) {
  test_t *freedata = NULL;

  freedata = (test_t *) data;
  SAFE_FREE(freedata);
}

static void setup(void) {
  fail_if(c_rbtree_create(&tree, key_cmp, data_cmp) < 0, "Setup failed");
}

static void setup_complete_tree(void) {
  int i = 0;

  fail_if(c_rbtree_create(&tree, key_cmp, data_cmp) < 0, "Setup failed");
  for (i = 0; i < 100; i++) {
    test_t *testdata = NULL;

    testdata = c_malloc(sizeof(test_t));
    fail_if(testdata == NULL, NULL);

    testdata->key = i;

    fail_if(c_rbtree_insert(tree, (void *) testdata) < 0, "Setup failed");
  }
}

static void teardown(void) {
  c_rbtree_destroy(tree, destructor);
  c_rbtree_free(tree);
  tree = NULL;
}

START_TEST (check_c_rbtree_create_free)
{
  fail_unless(c_rbtree_create(&tree, key_cmp, data_cmp) == 0, NULL);
  fail_unless(tree->size == 0, NULL);

  fail_unless(c_rbtree_free(tree) == 0, NULL);
  tree = NULL;
}
END_TEST

START_TEST (check_c_rbtree_create_null)
{
  fail_unless(c_rbtree_create(NULL, key_cmp, data_cmp) < 0, NULL);
  fail_unless(c_rbtree_create(&tree, NULL, data_cmp) < 0, NULL);
  fail_unless(c_rbtree_create(&tree, key_cmp, NULL) < 0, NULL);
}
END_TEST

START_TEST (check_c_rbtree_free_null)
{
  fail_unless(c_rbtree_free(NULL) < 0, NULL);
}
END_TEST

START_TEST (check_c_rbtree_insert_delete)
{
  c_rbnode_t *node = NULL;
  test_t *testdata = NULL;

  fail_unless(c_rbtree_create(&tree, key_cmp, data_cmp) == 0, NULL);

  testdata = c_malloc(sizeof(test_t));
  testdata->key = 42;

  fail_unless(c_rbtree_insert(tree, (void *) testdata) == 0, NULL);

  node = c_rbtree_head(tree);
  fail_if(node == NULL, NULL);

  testdata = c_rbtree_node_data(node);
  SAFE_FREE(testdata);
  fail_unless(c_rbtree_node_delete(node) == 0, NULL);

  c_rbtree_free(tree);
  tree = NULL;
}
END_TEST

START_TEST (check_c_rbtree_insert_random)
{
  int i = 0, rc = -1;

  for (i = 0; i < 100; i++) {
    test_t *testdata = NULL;

    testdata = c_malloc(sizeof(test_t));
    fail_if(testdata == NULL, NULL);

    testdata->key = i;

    rc = c_rbtree_insert(tree, testdata);
    fail_unless(rc == 0, "c_rbtree_insert failed for %d with return code %d", i, rc);

  }
  rc = c_rbtree_check_sanity(tree);
  fail_unless(rc == 0, "c_rbtree_check_sanity failed with return code %d", rc);
}
END_TEST

START_TEST (check_c_rbtree_insert_duplicate)
{
  test_t *testdata = NULL;

  testdata = c_malloc(sizeof(test_t));
  fail_if(testdata == NULL, NULL);

  testdata->key = 42;

  fail_unless(c_rbtree_insert(tree, (void *) testdata) == 0, NULL);

  /* add again */
  testdata = c_malloc(sizeof(test_t));
  fail_if(testdata == NULL, NULL);

  testdata->key = 42;

  /* check for duplicate */
  fail_unless(c_rbtree_insert(tree, (void *) testdata) == 1, NULL);
  SAFE_FREE(testdata);
}
END_TEST

START_TEST (check_c_rbtree_find)
{
  int rc = -1, i = 42;
  c_rbnode_t *node = NULL;
  test_t *testdata = NULL;

  rc = c_rbtree_check_sanity(tree);
  fail_unless(rc == 0, "c_rbtree_check_sanity failed with return code %d", rc);

  /* find the node with the key 42 */
  node = c_rbtree_find(tree, (void *) &i);
  fail_if(node == NULL, NULL);

  testdata = (test_t *) c_rbtree_node_data(node);
  fail_unless(testdata->key == 42, NULL);
}
END_TEST

START_TEST (check_c_rbtree_delete)
{
  int rc = -1, i = 42;
  c_rbnode_t *node = NULL;
  test_t *freedata = NULL;

  rc = c_rbtree_check_sanity(tree);
  fail_unless(rc == 0, "c_rbtree_check_sanity failed with return code %d", rc);

  node = c_rbtree_find(tree, (void *) &i);
  fail_if(node == NULL, NULL);

  freedata = (test_t *) c_rbtree_node_data(node);
  SAFE_FREE(freedata);
  fail_unless(c_rbtree_node_delete(node) == 0, NULL);

  rc = c_rbtree_check_sanity(tree);
  fail_unless(rc == 0, "c_rbtree_check_sanity failed with return code %d", rc);
}
END_TEST

START_TEST (check_c_rbtree_walk)
{
  int rc = -1, i = 42;
  test_t *testdata = NULL;
  c_rbnode_t *node = NULL;

  rc = c_rbtree_check_sanity(tree);
  fail_unless(rc == 0, "c_rbtree_check_sanity failed with return code %d", rc);

  testdata = (test_t *) c_malloc(sizeof(test_t));
  testdata->key = 42;

  rc = c_rbtree_walk(tree, testdata, visitor);
  fail_unless(rc == 0, NULL);

  /* find the node with the key 42 */
  node = c_rbtree_find(tree, (void *) &i);
  fail_if(node == NULL, NULL);
  SAFE_FREE(testdata);

  testdata = (test_t *) c_rbtree_node_data(node);
  fail_unless(testdata->number == 42, NULL);

}
END_TEST

START_TEST (check_c_rbtree_dup)
{
  int rc = -1;
  c_rbtree_t *duptree = NULL;

  duptree = c_rbtree_dup(tree);
  fail_if(duptree == NULL, NULL);

  rc = c_rbtree_check_sanity(duptree);
  fail_unless(rc == 0, "c_rbtree_check_sanity failed with return code %d", rc);

  c_rbtree_free(duptree);
}
END_TEST

#if 0
START_TEST (check_c_rbtree_x)
{
  int rc = -1;

  rc = c_rbtree_check_sanity(tree);
  fail_unless(rc == 0, "c_rbtree_check_sanity failed with return code %d", rc);
}
END_TEST
#endif

static Suite *make_c_rbtree_suite(void) {
  Suite *s = suite_create("std:rbtree");

  create_case(s, "check_c_rbtree_create_free", check_c_rbtree_create_free);
  create_case(s, "check_c_rbtree_create_null", check_c_rbtree_create_null);
  create_case(s, "check_c_rbtree_free_null", check_c_rbtree_free_null);
  create_case(s, "check_c_rbtree_insert_delete", check_c_rbtree_insert_delete);
  create_case_fixture(s, "check_c_rbtree_insert_random", check_c_rbtree_insert_random, setup, teardown);
  create_case_fixture(s, "check_c_rbtree_insert_duplicate", check_c_rbtree_insert_duplicate, setup, teardown);
  create_case_fixture(s, "check_c_rbtree_find", check_c_rbtree_find, setup_complete_tree, teardown);
  create_case_fixture(s, "check_c_rbtree_delete", check_c_rbtree_delete, setup_complete_tree, teardown);
  create_case_fixture(s, "check_c_rbtree_walk", check_c_rbtree_walk, setup_complete_tree, teardown);
  create_case_fixture(s, "check_c_rbtree_dup", check_c_rbtree_dup, setup_complete_tree, teardown);

  return s;
}

int main(int argc, char **argv) {
  Suite *s = NULL;
  SRunner *sr = NULL;
  struct argument_s arguments;
  int nf;

  ZERO_STRUCT(arguments);

  cmdline_parse(argc, argv, &arguments);

  s = make_c_rbtree_suite();

  sr = srunner_create(s);
  if (arguments.nofork) {
    srunner_set_fork_status(sr, CK_NOFORK);
  }
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

