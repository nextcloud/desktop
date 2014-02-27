/*
 * httpbf - send big files via http
 *
 * Copyright (c) 2012 Klaas Freitag <freitag@owncloud.com>
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

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>

#include <cmocka.h>

#include "config_csync.h"

#include <httpbf.h>


// A test case that does nothing and succeeds.
static void null_test_success(void **state) {
    (void) state;
}


static char* test_file( const char* name ) {
    if( ! name ) return 0;

    char path[260];
    strcpy( path, TESTFILEDIR);
    if(path[strlen(TESTFILEDIR)-1] != '/')
        strcat( path, "/");
    strcat( path, name );

    return strdup(path);
}

static void test_get_transfer_url( void **state ) {
    const char *url = "http://example.org/owncloud";
    const char *turl = NULL;
    char res[256];
    int i;
    Hbf_State hbf_state;

    hbf_transfer_t *list = NULL;
    list = hbf_init_transfer( url );
    assert_non_null( list );

    /* open a file */
    int fd = open( test_file("church.jpg"), O_RDONLY );
    assert_true(fd >= 0);

    hbf_state = hbf_splitlist(list, fd);
    assert_true( hbf_state == HBF_SUCCESS);

    for( i=0; i < list->block_cnt; i++ ) {
        turl = get_transfer_url( list, i );

        sprintf(res, "%s-chunking-%d-%d-%d", url, list->transfer_id,
                list->block_cnt, i );
        printf( "XX: %s\n", res );
        assert_string_equal( turl, res );

    }
}

static void test_hbf_init_transfer( void **state ) {
    hbf_transfer_t *list = NULL;
    const char *url = "http://example.org/owncloud";

    list = hbf_init_transfer( url );
    assert_non_null( list );
    assert_string_equal( url, list->url );
}

/* test with a file size that is not a multiply of the slize size. */
static void test_hbf_splitlist_odd( void **state ){

    hbf_transfer_t *list = NULL;
    const char *dest_url = "http://localhost/ocm/remote.php/webdav/big/church.jpg";

    /* open a file */
    int fd = open(test_file("church.jpg"), O_RDONLY);
    assert_true(fd >= 0);

    int prev_id = 0;
    int i;

    Hbf_State hbf_state;

    /* do a smoke test for uniqueness */
    for( i=0; i < 10000; i++) {
        list = hbf_init_transfer(dest_url);
        assert_non_null(list);
        usleep(1);
        hbf_state = hbf_splitlist(list, fd);

        assert_int_not_equal(list->transfer_id, prev_id);
        prev_id = list->transfer_id;
        hbf_free_transfer(list);
    }

    list = hbf_init_transfer(dest_url);
    assert_non_null(list);

    hbf_state = hbf_splitlist(list, fd);
    assert_non_null(list);
    assert_int_equal(list->calc_size, list->stat_size);
    assert_int_not_equal(list->block_cnt, 0);
    assert_true( hbf_state == HBF_SUCCESS);

    /* checks on the block list */
    int seen_zero_seq = 0;
    int prev_seq = -1;
    int64_t prev_block_end = -1;

    for( i=0; i < list->block_cnt; i++) {
        hbf_block_t *blk = list->block_arr[i];
        assert_non_null(blk);
        if( blk->seq_number == 0 ) seen_zero_seq++;

        assert_int_equal(prev_seq, blk->seq_number -1 );
        prev_seq = blk->seq_number;

        assert_true((prev_block_end+1) == (blk->start));
        prev_block_end = blk->start + blk->size;
    }
    /* Make sure we exactly saw blk->seq_number == 0 exactly one times */
    assert_int_equal( seen_zero_seq, 1 );

    hbf_free_transfer( list );
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(null_test_success),
        unit_test(test_hbf_splitlist_odd),
        unit_test(test_hbf_init_transfer),
        unit_test(test_get_transfer_url)
    };
    return run_tests(tests);
}
