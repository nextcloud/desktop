#include <string.h>

#include "support.h"

#include "std/c_path.h"

START_TEST (check_c_basename)
{
  char *bname;

  bname = c_basename("/usr/lib");
  fail_unless((strcmp(bname, "lib") == 0), NULL);
  SAFE_FREE(bname);

  bname = c_basename("/usr//");
  fail_unless((strcmp(bname, "usr") == 0), NULL);
  SAFE_FREE(bname);

  bname = c_basename("usr");
  fail_unless((strcmp(bname, "usr") == 0), NULL);
  SAFE_FREE(bname);

  bname = c_basename("///");
  fail_unless((strcmp(bname, "/") == 0), NULL);
  SAFE_FREE(bname);

  bname = c_basename("/");
  fail_unless((strcmp(bname, "/") == 0), NULL);
  SAFE_FREE(bname);

  bname = c_basename(".");
  fail_unless((strcmp(bname, ".") == 0), NULL);
  SAFE_FREE(bname);

  bname = c_basename("..");
  fail_unless((strcmp(bname, "..") == 0), NULL);
  SAFE_FREE(bname);

  bname = c_basename("");
  fail_unless((strcmp(bname, ".") == 0), NULL);
  SAFE_FREE(bname);

  bname = c_basename(NULL);
  fail_unless((strcmp(bname, ".") == 0), NULL);
  SAFE_FREE(bname);

}
END_TEST

START_TEST (check_c_dirname)
{
  char *dname;

  dname = c_dirname("/usr/lib");
  fail_unless((strcmp(dname, "/usr") == 0), "c_dirname = %s\n", dname);
  SAFE_FREE(dname);

  dname = c_dirname("/usr//");
  fail_unless((strcmp(dname, "/") == 0), "c_dirname = %s\n", dname);
  SAFE_FREE(dname);

  dname = c_dirname("usr");
  fail_unless((strcmp(dname, ".") == 0), "c_dirname = %s\n", dname);
  SAFE_FREE(dname);

  dname = c_dirname("/");
  fail_unless((strcmp(dname, "/") == 0), NULL);
  SAFE_FREE(dname);

  dname = c_dirname("///");
  fail_unless((strcmp(dname, "/") == 0), NULL);
  SAFE_FREE(dname);

  dname = c_dirname(".");
  fail_unless((strcmp(dname, ".") == 0), NULL);
  SAFE_FREE(dname);

  dname = c_dirname("..");
  fail_unless((strcmp(dname, ".") == 0), NULL);
  SAFE_FREE(dname);

  dname = c_dirname(NULL);
  fail_unless((strcmp(dname, ".") == 0), NULL);
  SAFE_FREE(dname);
}
END_TEST

static Suite *make_std_c_basename_suite(void) {
  Suite *s = suite_create("std:path:c_basename");

  create_case(s, "check_c_basename", check_c_basename);

  return s;
}

static Suite *make_std_c_dirname_suite(void) {
  Suite *s = suite_create("std:path:c_dirname");

  create_case(s, "check_c_dirname", check_c_dirname);

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

  s = make_std_c_basename_suite();
  s2 = make_std_c_dirname_suite();

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

