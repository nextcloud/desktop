#include "torture.h"

#include "c_string.h"

#ifdef _WIN32
#include <string.h>
#endif

static void setup(void **state)
{
    int rc = 0;

    (void) state; /* unused */

#ifdef HAVE_ICONV
#ifdef __APPLE__
    /* this test only works on apple because linux does not know the
     * UTF-8-MAC encoding that we use here. */
    rc = c_setup_iconv("UTF-8-MAC");
#endif
#endif
    assert_int_equal(rc, 0);
}

static void teardown(void **state)
{
    int rc = 0;

    (void) state; /* unused */
#ifdef HAVE_ICONV
    // this must never crash
    rc = c_close_iconv();
#endif
    assert_int_equal(rc, 0);
}

static void check_iconv_to_native_normalization(void **state)
{
    mbchar_t *out = NULL;
    const char *in= "\x48\xc3\xa4"; // UTF8
#ifdef __APPLE__
    const char *exp_out = "\x48\x61\xcc\x88"; // UTF-8-MAC
#else
    const char *exp_out = "\x48\xc3\xa4"; // UTF8
#endif

    out = c_utf8_to_locale(in);
    assert_string_equal(out, exp_out);

    c_free_locale_string(out);
    assert_null(out);

    (void) state; /* unused */
}

static void check_iconv_from_native_normalization(void **state)
{
    char *out = NULL;
#ifdef _WIN32
    const mbchar_t *in = L"\x48\xc3\xa4"; // UTF-8
#else
#ifdef __APPLE__
    const mbchar_t *in = "\x48\x61\xcc\x88"; // UTF-8-MAC
#else
    const mbchar_t *in = "\x48\xc3\xa4"; // UTF-8
#endif
#endif
    const char *exp_out = "\x48\xc3\xa4"; // UTF-8

    out = c_utf8_from_locale(in);
    assert_string_equal(out, exp_out);

    c_free_locale_string(out);
    assert_null(out);

    (void) state; /* unused */
}

static void check_iconv_ascii(void **state)
{
#ifdef _WIN32
    const mbchar_t *in = L"abc/ABC\\123"; // UTF-8
#else
#ifdef __APPLE__
    const mbchar_t *in = "abc/ABC\\123"; // UTF-8-MAC
#else
    const mbchar_t *in = "abc/ABC\\123"; // UTF-8
#endif
#endif
    char *out = NULL;
    const char *exp_out = "abc/ABC\\123";

    out = c_utf8_from_locale(in);
    assert_string_equal(out, exp_out);

    c_free_locale_string(out);
    assert_null(out);

    (void) state; /* unused */
}

#define TESTSTRING "#cA\\#fß§4"
#define LTESTSTRING L"#cA\\#fß§4"

static void check_to_multibyte(void **state)
{
    int rc = -1;

    mbchar_t *mb_string = c_utf8_to_locale( TESTSTRING );
    mbchar_t *mb_null   = c_utf8_to_locale( NULL );

    (void) state;

#ifdef _WIN32
    assert_int_equal( wcscmp( LTESTSTRING, mb_string), 0 );
#else
    assert_string_equal(mb_string, TESTSTRING);
#endif
    assert_true( mb_null == NULL );
    assert_int_equal(rc, -1);

    c_free_locale_string(mb_string);
    c_free_locale_string(mb_null);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_to_multibyte,                    setup, teardown),
        unit_test_setup_teardown(check_iconv_ascii,                     setup, teardown),
        unit_test_setup_teardown(check_iconv_to_native_normalization,   setup, teardown),
        unit_test_setup_teardown(check_iconv_from_native_normalization, setup, teardown),
    };

    return run_tests(tests);
}

