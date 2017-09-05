/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2013 by Klaas Freitag <freitag@owncloud.com>
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
#include "torture.h"
#include <stdio.h>
#include "c_string.h"
#include "c_path.h"
#include "c_utf8.h"

#ifdef _WIN32
#include <string.h>
#endif


static void check_iconv_to_native_normalization(void **state)
{
    mbchar_t *out = NULL;
    const char *in= "\x48\xc3\xa4"; // UTF8
#ifdef __APPLE__
    const char *exp_out = "\x48\x61\xcc\x88"; // UTF-8-MAC
#else
    const char *exp_out = "\x48\xc3\xa4"; // UTF8
#endif

    out = c_utf8_path_to_locale(in);
    assert_string_equal(out, exp_out);

    c_free_locale_string(out);
    assert_null(out);

    (void) state; /* unused */
}

static void check_iconv_from_native_normalization(void **state)
{
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

    QByteArray out = c_utf8_from_locale(in);
    assert_string_equal(out, exp_out);

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
    const char *exp_out = "abc/ABC\\123";

    QByteArray out = c_utf8_from_locale(in);
    assert_string_equal(out, exp_out);

    (void) state; /* unused */
}

#define TESTSTRING "#cA\\#fß§4"
#define LTESTSTRING L"#cA\\#fß§4"

static void check_to_multibyte(void **state)
{
    int rc = -1;

    mbchar_t *mb_string = c_utf8_path_to_locale( TESTSTRING );
    mbchar_t *mb_null   = c_utf8_path_to_locale( NULL );

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

static void check_long_win_path(void **state)
{
    (void) state; /* unused */

    {
        const char *path = "C://DATA/FILES/MUSIC/MY_MUSIC.mp3"; // check a short path
        const char *exp_path = "\\\\?\\C:\\\\DATA\\FILES\\MUSIC\\MY_MUSIC.mp3";
        const char *new_short = c_path_to_UNC(path);
        assert_string_equal(new_short, exp_path);
        SAFE_FREE(new_short);
    }

    {
        const char *path = "\\\\foo\\bar/MY_MUSIC.mp3";
        const char *exp_path = "\\\\foo\\bar\\MY_MUSIC.mp3";
        const char *new_short = c_path_to_UNC(path);
        assert_string_equal(new_short, exp_path);
        SAFE_FREE(new_short);
    }

    {
        const char *path = "//foo\\bar/MY_MUSIC.mp3";
        const char *exp_path = "\\\\foo\\bar\\MY_MUSIC.mp3";
        const char *new_short = c_path_to_UNC(path);
        assert_string_equal(new_short, exp_path);
        SAFE_FREE(new_short);
    }

    {
        const char *path = "\\foo\\bar";
        const char *exp_path = "\\\\?\\foo\\bar";
        const char *new_short = c_path_to_UNC(path);
        assert_string_equal(new_short, exp_path);
        SAFE_FREE(new_short);
    }

    {
        const char *path = "/foo/bar";
        const char *exp_path = "\\\\?\\foo\\bar";
        const char *new_short = c_path_to_UNC(path);
        assert_string_equal(new_short, exp_path);
        SAFE_FREE(new_short);
    }

    const char *longPath = "D://alonglonglonglong/blonglonglonglong/clonglonglonglong/dlonglonglonglong/"
            "elonglonglonglong/flonglonglonglong/glonglonglonglong/hlonglonglonglong/ilonglonglonglong/"
            "jlonglonglonglong/klonglonglonglong/llonglonglonglong/mlonglonglonglong/nlonglonglonglong/"
            "olonglonglonglong/file.txt";
    const char *longPathConv = "\\\\?\\D:\\\\alonglonglonglong\\blonglonglonglong\\clonglonglonglong\\dlonglonglonglong\\"
            "elonglonglonglong\\flonglonglonglong\\glonglonglonglong\\hlonglonglonglong\\ilonglonglonglong\\"
            "jlonglonglonglong\\klonglonglonglong\\llonglonglonglong\\mlonglonglonglong\\nlonglonglonglong\\"
            "olonglonglonglong\\file.txt";

    const char *new_long = c_path_to_UNC(longPath);
    // printf( "XXXXXXXXXXXX %s %d\n", new_long, mem_reserved);

    assert_string_equal(new_long, longPathConv);

    // printf( "YYYYYYYYYYYY %ld\n", strlen(new_long));
    assert_int_equal( strlen(new_long), 286);
    SAFE_FREE(new_long);
}

int torture_run_tests(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(check_long_win_path),
        cmocka_unit_test(check_to_multibyte),
        cmocka_unit_test(check_iconv_ascii),
        cmocka_unit_test(check_iconv_to_native_normalization),
        cmocka_unit_test(check_iconv_from_native_normalization),

    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

