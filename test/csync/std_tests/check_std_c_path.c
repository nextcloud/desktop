/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
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
  };

  return run_tests(tests);
}

