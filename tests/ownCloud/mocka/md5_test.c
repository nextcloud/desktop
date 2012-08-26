/*
 * Copyright 2012 Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 */


#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <iniparser.h>
#include <std/c_path.h>

#include <csync_util.h>

#include <cmocka.h>

#include "config_test.h"


static void test_testdir_exists(void **state) {
    csync_stat_t sb;
    const _TCHAR *wuri = c_multibyte( TESTFILES_DIR );

    (void) state;
    assert_int_equal( _tstat(wuri, &sb), 0 );
    assert_true( S_ISDIR(sb.st_mode));

    c_free_multibyte(wuri);
}

static void test_md5_buffer(void **state) {
    const char *t1 = "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.";
    const char *t2 = "This is a nice md5 test";
    const char *t3 = "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet."
            "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.";

    (void) state;
    assert_string_equal( csync_buffer_md5(t1, strlen(t1)), "dbcb1ed05ecf975f532604f2d2000246");
    assert_string_equal( csync_buffer_md5(t2, strlen(t2)), "65236ae09754bca371fb869384040141");
    assert_string_equal( csync_buffer_md5(t3, strlen(t3)), "651f37892a9df60ed087bc7a1c660fec");
}

static void test_md5_files(void **state) {
    char path[255];
    (void) state;

    strcpy(path, TESTFILES_DIR);
    strcat(path, "test.txt");
    assert_string_equal( csync_file_md5(path), "f3971ce599093756e6018513d0835134");

    strcpy(path, TESTFILES_DIR);
    strcat(path, "red_is_the_rose.jpg");
    assert_string_equal( csync_file_md5(path), "baf8eeb2a36af94f033fa0094c50c2d5");

}


int main(void) {
    const UnitTest tests[] = {
        unit_test(test_testdir_exists),
        unit_test(test_md5_buffer),
        unit_test(test_md5_files)
    };

    return run_tests(tests);
}
