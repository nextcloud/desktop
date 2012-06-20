#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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

START_TEST (check_c_basename_uri)
{
  char *bname = NULL;

  bname = c_basename("smb://server/share/dir/");
  fail_unless((strcmp(bname, "dir") == 0), NULL);
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

START_TEST (check_c_dirname_uri)
{
  char *dname;

  dname = c_dirname("smb://server/share/dir");
  fail_unless((strcmp(dname, "smb://server/share") == 0), "c_dirname = %s\n", dname);
  SAFE_FREE(dname);
}
END_TEST

START_TEST (check_c_tmpname)
{
  char tmpl[22]={0};
  char prev[22]={0};

  int i=0;
  srand((unsigned)time(NULL));

  /* remember the last random value and compare the new one against.
   * They may never be the same. */
  for(i = 0; i < 100; i++){
      strcpy(tmpl, "check_tmpname.XXXXXX");
      fail_unless(c_tmpname(tmpl) == 0);

      if(strlen(prev)) {
          fail_if(strcmp(tmpl, prev) == 0);
      }
      strcpy(prev,tmpl);

  }
}
END_TEST

START_TEST (check_c_parse_uri)
{
  const char *test_scheme = "git+ssh";
  const char *test_user = "gladiac";
  const char *test_passwd = "secret";
  const char *test_host = "git.csync.org";
  const char *test_path = "/srv/git/csync.git";

  char *uri = NULL;

  char *scheme = NULL;
  char *user = NULL;
  char *passwd = NULL;
  char *host = NULL;
  unsigned int port;
  char *path = NULL;

  fail_if(asprintf(&uri, "%s://%s:%s@%s:22%s",
      test_scheme, test_user, test_passwd, test_host, test_path) < 0, NULL);

  fail_unless(c_parse_uri(uri, &scheme, &user, &passwd, &host, &port, &path) == 0, NULL);

  fail_unless(strcmp(test_scheme, scheme) == 0, NULL);
  fail_unless(strcmp(test_user, user) == 0, NULL);
  fail_unless(strcmp(test_passwd, passwd) == 0, NULL);
  fail_unless(strcmp(test_host, host) == 0, NULL);
  fail_unless(port == 22, NULL);
  fail_unless(strcmp(test_path, path) == 0, NULL);

  SAFE_FREE(uri);
  SAFE_FREE(scheme);
  SAFE_FREE(user);
  SAFE_FREE(passwd);
  SAFE_FREE(host);
  SAFE_FREE(path);
}
END_TEST

static Suite *make_std_c_basename_suite(void) {
  Suite *s = suite_create("std:path:c_basename");

  create_case(s, "check_c_basename", check_c_basename);
  create_case(s, "check_c_basename_uri", check_c_basename_uri);

  return s;
}

static Suite *make_std_c_dirname_suite(void) {
  Suite *s = suite_create("std:path:c_dirname");

  create_case(s, "check_c_dirname", check_c_dirname);
  create_case(s, "check_c_dirname_uri", check_c_dirname_uri);

  return s;
}

static Suite *make_std_c_parse_uri_suite(void) {
  Suite *s = suite_create("std:path:c_parse_uri");

  create_case(s, "check_c_parse_uri", check_c_parse_uri);

  return s;
}

static Suite *make_std_c_tmpname_suite(void) {
  Suite *s = suite_create("std:path:c_tmpname");

  create_case(s, "check_c_tmpname", check_c_tmpname);

  return s;
}

int main(int argc, char **argv) {
  Suite *s, *s2, *s3, *s4;
  SRunner *sr = NULL;
  struct argument_s arguments;
  int nf;

  ZERO_STRUCT(arguments);

  cmdline_parse(argc, argv, &arguments);

  s = make_std_c_basename_suite();
  s2 = make_std_c_dirname_suite();
  s3 = make_std_c_parse_uri_suite();
  s4 = make_std_c_tmpname_suite();

  sr = srunner_create(s);
  if (arguments.nofork) {
    srunner_set_fork_status(sr, CK_NOFORK);
  }
  srunner_add_suite (sr, s2);
  srunner_add_suite (sr, s3);
  srunner_add_suite (sr, s4);
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

