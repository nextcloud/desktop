#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "torture.h"

#include "std/c_string.h"

static void check_c_streq_equal(void **state)
{
    (void) state; /* unused */

    assert_true(c_streq("test", "test"));
}

static void check_c_streq_not_equal(void **state)
{
    (void) state; /* unused */

    assert_false(c_streq("test", "test2"));
}

static void check_c_streq_null(void **state)
{
    (void) state; /* unused */

    assert_false(c_streq(NULL, "test"));
    assert_false(c_streq("test", NULL));
    assert_false(c_streq(NULL, NULL));
}

static void check_c_strlist_new(void **state)
{
    c_strlist_t *strlist = NULL;

    (void) state; /* unused */

    strlist = c_strlist_new(42);
    assert_non_null(strlist);
    assert_int_equal(strlist->size, 42);
    assert_int_equal(strlist->count, 0);

    c_strlist_destroy(strlist);
}

static void check_c_strlist_add(void **state)
{
    int rc;
    size_t i = 0;
    c_strlist_t *strlist = NULL;

    (void) state; /* unused */

    strlist = c_strlist_new(42);
    assert_non_null(strlist);
    assert_int_equal(strlist->size, 42);
    assert_int_equal(strlist->count, 0);

    for (i = 0; i < strlist->size; i++) {
        rc = c_strlist_add(strlist, (char *) "foobar");
        assert_int_equal(rc, 0);
    }

    assert_int_equal(strlist->count, 42);
    assert_string_equal(strlist->vector[0], "foobar");
    assert_string_equal(strlist->vector[41], "foobar");

    c_strlist_destroy(strlist);
}

static void check_c_strlist_expand(void **state)
{
    c_strlist_t *strlist;
    size_t i = 0;
    int rc;

    (void) state; /* unused */

    strlist = c_strlist_new(42);
    assert_non_null(strlist);
    assert_int_equal(strlist->size, 42);
    assert_int_equal(strlist->count, 0);

    strlist = c_strlist_expand(strlist, 84);
    assert_non_null(strlist);
    assert_int_equal(strlist->size, 84);

    for (i = 0; i < strlist->size; i++) {
        rc = c_strlist_add(strlist, (char *) "foobar");
        assert_int_equal(rc, 0);
    }

    c_strlist_destroy(strlist);
}

static void check_c_strreplace(void **state)
{
    char *str = strdup("/home/%(USER)");

    (void) state; /* unused */

    str = c_strreplace(str, "%(USER)", "csync");
    assert_string_equal(str,  "/home/csync");

    free(str);
}

static void check_c_lowercase(void **state)
{
    char *str;

    (void) state; /* unused */

    str = c_lowercase("LoWeRcASE");
    assert_string_equal(str,  "lowercase");

    free(str);
}

static void check_c_lowercase_empty(void **state)
{
    char *str;

    (void) state; /* unused */

    str = c_lowercase("");
    assert_string_equal(str,  "");

    free(str);
}

static void check_c_lowercase_null(void **state)
{
    char *str;

    (void) state; /* unused */

    str = c_lowercase(NULL);
    assert_null(str);
}

static void check_c_uppercase(void **state)
{
    char *str;

    (void) state; /* unused */

    str = c_uppercase("upperCASE");
    assert_string_equal(str,  "UPPERCASE");

    free(str);
}

static void check_c_uppercase_empty(void **state)
{
    char *str;

    (void) state; /* unused */

    str = c_uppercase("");
    assert_string_equal(str,  "");

    free(str);
}

static void check_c_uppercase_null(void **state)
{
    char *str;

    (void) state; /* unused */

    str = c_uppercase(NULL);
    assert_null(str);
}

static void check_iconv_setup(void **state)
{
    int rc = 0;

    (void) state; /* unused */

#ifdef __APPLE__
    rc = c_setup_iconv("UTF-8-MAC");
    assert_int_equal(rc, 0);
#endif
}

static void check_iconv_teardown(void **state)
{
    int rc = 0;

    (void) state; /* unused */

    // this must never crash
    rc = c_close_iconv();
    assert_int_equal(rc, 0);
}

static void check_iconv_to_native_normalization(void **state)
{
    const char *out = NULL;
    const char *in= "\x48\xc3\xa4"; // UTF8
#ifdef __APPLE__
    const char *exp_out = "\x48\x61\xcc\x88"; // UTF-8-MAC
#else
    const char *exp_out = "\x48\xc3\xa4"; // UTF8
#endif

    out = c_multibyte(in);
    assert_string_equal(out, exp_out);

    c_free_multibyte(out);
    assert_null(out);

    (void) state; /* unused */
}

static void check_iconv_from_native_normalization(void **state)
{
    const char *out = NULL;
#ifdef __APPLE__
    const char* in = "\x48\x61\xcc\x88"; // UTF-8-MAC
#else
    const char *in = "\x48\xc3\xa4"; // UTF-8
#endif
    const char *exp_out = "\x48\xc3\xa4"; // UTF-8

    out = c_utf8(in);
    assert_string_equal(out, exp_out);

    c_free_utf8(out);
    assert_null(out);

    (void) state; /* unused */
}

static void check_iconv_ascii(void **state)
{
    const char *out = NULL;
    const char* in = "abc/ABC\\123";
    const char *exp_out = "abc/ABC\\123";

    out = c_utf8(in);
    assert_string_equal(out, exp_out);

    c_free_utf8(out);
    assert_null(out);

    (void) state; /* unused */
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test(check_c_streq_equal),
        unit_test(check_c_streq_not_equal),
        unit_test(check_c_streq_null),
        unit_test(check_c_strlist_new),
        unit_test(check_c_strlist_add),
        unit_test(check_c_strlist_expand),
        unit_test(check_c_strreplace),
        unit_test(check_c_lowercase),
        unit_test(check_c_lowercase_empty),
        unit_test(check_c_lowercase_null),
        unit_test(check_c_uppercase),
        unit_test(check_c_uppercase_empty),
        unit_test(check_c_uppercase_null),
        unit_test_setup_teardown(check_iconv_ascii, check_iconv_setup, check_iconv_teardown),
        unit_test_setup_teardown(check_iconv_to_native_normalization, check_iconv_setup, check_iconv_teardown),
        unit_test_setup_teardown(check_iconv_from_native_normalization, check_iconv_setup, check_iconv_teardown),
    };

    return run_tests(tests);
}

