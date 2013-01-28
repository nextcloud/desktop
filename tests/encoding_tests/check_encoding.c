#include "torture.h"

#include "c_string.h"

#ifdef _WIN32
#include <string.h>
#endif

static void setup(void **state)
{

}

static void teardown(void **state)
{

}

#define TESTSTRING "#äA\\#fß§4"
#ifdef _WIN32
#define LTESTSTRING L"#äA\\#fß§4"
#endif

static void check_to_multibyte(void **state)
{
    int rc = -1;

    const mbchar_t *mb_string = c_multibyte( TESTSTRING );
    const mbchar_t *mb_null   = c_multibyte( NULL );
#ifdef _WIN32
    assert_int_equal( wcscmp( LTESTSTRING, mb_string), 0 );
#else
    assert_string_equal(mb_string, TESTSTRING);
#endif
    assert_true( mb_null == NULL );
    assert_int_equal(rc, -1);
}

static void check_to_utf8( void **state)
{

}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_to_multibyte, setup, teardown),
        unit_test_setup_teardown(check_to_utf8, setup, teardown)
    };

    return run_tests(tests);
}

