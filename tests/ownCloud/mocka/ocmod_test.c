/*
 * Copyright 2008 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */



#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "../../modules/csync_owncloud.c"
#include <cmocka.h>

// A test case that does nothing and succeeds.
static void null_test_success(void **state) {
    (void) state;
}


static void connect_test_success(void **state) {
    const char *url = "owncloud://kf:12345@localhost/oc";
    int re;
    
    re = dav_connect( url );
    assert_int_equal( re, 0 );
    assert_int_equal( _connected, 1 );
    assert_int_equal( dav_session.time_delta_sum, 0);
    assert_int_equal( dav_session.time_delta_cnt, 0);
}

static void fetch_a_context() {
    struct listdir_context  *fetchCtx = NULL;
    char *curi = _cleanPath("http://localhost/oc/files/webdav.php/");
    
    int rc = 0;
    
    fetchCtx = c_malloc( sizeof( struct listdir_context ));
    fetchCtx->target = curi;
    fetchCtx->include_target = 1;
    
    rc = fetch_resource_list( curi, NE_DEPTH_ONE, fetchCtx );
    assert_int_equal( rc, 0 );
    printf("Results: %d\n", fetchCtx->result_count);
    
    fetchCtx->currResource = fetchCtx->list;
    for( int i = 0; i < fetchCtx->result_count; i++ ) {
	assert_true( fetchCtx->currResource != NULL );
	assert_true( fetchCtx->currResource->uri != NULL );
	assert_true( fetchCtx->currResource->name != NULL );
	
	printf( "   %s -> %s\n", fetchCtx->currResource->uri, fetchCtx->currResource->name );
	fetchCtx->currResource = fetchCtx->currResource->next;
    }
    
}


int main(void) {
    const UnitTest tests[] = {
        unit_test(null_test_success),
        unit_test(connect_test_success),
        unit_test(fetch_a_context),
    };

    return run_tests(tests);
}
