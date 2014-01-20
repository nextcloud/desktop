#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "torture.h"

#include "std/c_path.h"

static void check_c_basename(void **state)
{
    char *bname;

    (void) state; /* unused */

    bname = c_basename("/usr/lib");
    assert_string_equal(bname, "lib");
    free(bname);

    bname = c_basename("/usr//");
    assert_string_equal(bname, "usr");
    free(bname);

    bname = c_basename("usr");
    assert_string_equal(bname, "usr");
    free(bname);

    bname = c_basename("///");
    assert_string_equal(bname, "/");
    free(bname);

    bname = c_basename("/");
    assert_string_equal(bname, "/");
    free(bname);

    bname = c_basename(".");
    assert_string_equal(bname, ".");
    free(bname);

    bname = c_basename("..");
    assert_string_equal(bname, "..");
    free(bname);

    bname = c_basename("");
    assert_string_equal(bname, ".");
    free(bname);

    bname = c_basename(NULL);
    assert_string_equal(bname, ".");
    free(bname);
}

static void check_c_basename_uri(void **state)
{
    char *bname = NULL;

    (void) state; /* unused */

    bname = c_basename("smb://server/share/dir/");
    assert_string_equal(bname, "dir");
    free(bname);
}

static void check_c_dirname(void **state)
{
    char *dname;

    (void) state; /* unused */

    dname = c_dirname("/usr/lib");
    assert_string_equal(dname, "/usr");
    free(dname);

    dname = c_dirname("/usr//");
    assert_string_equal(dname, "/");
    free(dname);

    dname = c_dirname("usr");
    assert_string_equal(dname, ".");
    free(dname);

    dname = c_dirname("/");
    assert_string_equal(dname, "/");
    free(dname);

    dname = c_dirname("///");
    assert_string_equal(dname, "/");
    free(dname);

    dname = c_dirname(".");
    assert_string_equal(dname, ".");
    free(dname);

    dname = c_dirname("..");
    assert_string_equal(dname, ".");
    free(dname);

    dname = c_dirname(NULL);
    assert_string_equal(dname, ".");
    free(dname);
}

static void check_c_dirname_uri(void **state)
{
    char *dname;

    (void) state; /* unused */

    dname = c_dirname("smb://server/share/dir");
    assert_string_equal(dname, "smb://server/share");
    free(dname);
}

static void check_c_tmpname(void **state)
{
    char tmpl[22]={0};
    char prev[22]={0};
    char *tmp;
    int i = 0;

    (void) state; /* unused */

    srand((unsigned)time(NULL));

    /* remember the last random value and compare the new one against.
     * They may never be the same. */
    for(i = 0; i < 100; i++){
        strcpy(tmpl, "check_tmpname.XXXXXX");
        tmp = c_tmpname(tmpl);
        assert_non_null(tmp);

        if (strlen(prev)) {
            assert_string_not_equal(tmp, prev);
        }
        strcpy(prev, tmp);
        SAFE_FREE(tmp);
    }
}

static void check_c_parse_uri(void **state)
{
    const char *test_scheme = "git+ssh";
    const char *test_user = "gladiac";
    const char *test_passwd = "secret";
    const char *test_host = "git.csync.org";
    const char *test_path = "/srv/git/csync.git";

    char *scheme = NULL;
    char *user = NULL;
    char *passwd = NULL;
    char *host = NULL;
    unsigned int port;
    char *path = NULL;
    char uri[1024] = {0};
    int rc;

    (void) state; /* unused */

    rc = snprintf(uri, sizeof(uri), "%s://%s:%s@%s:22%s",
                  test_scheme, test_user, test_passwd, test_host, test_path);
    assert_true(rc);

    rc = c_parse_uri(uri, &scheme, &user, &passwd, &host, &port, &path);
    assert_int_equal(rc, 0);

    assert_string_equal(test_scheme, scheme);
    assert_string_equal(test_user, user);
    assert_string_equal(test_passwd, passwd);
    assert_string_equal(test_host, host);
    assert_int_equal(port, 22);
    assert_string_equal(test_path, path);

    free(scheme);
    free(user);
    free(passwd);
    free(host);
    free(path);
}

int torture_run_tests(void)
{
  const UnitTest tests[] = {
      unit_test(check_c_basename),
      unit_test(check_c_basename_uri),
      unit_test(check_c_dirname),
      unit_test(check_c_dirname_uri),
      unit_test(check_c_parse_uri),
      unit_test(check_c_tmpname),
  };

  return run_tests(tests);
}

