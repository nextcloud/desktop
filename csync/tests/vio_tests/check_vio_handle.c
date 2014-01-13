#include <stdlib.h>

#include "torture.h"

#include "vio/csync_vio_handle.h"
#include "vio/csync_vio_handle_private.h"

static void check_csync_vio_handle_new(void **state)
{
    int *number;
    csync_vio_handle_t *handle;

    (void) state; /* unused */

    number = malloc(sizeof(int));
    *number = 42;

    handle = csync_vio_handle_new("/tmp", (csync_vio_method_handle_t *) number);
    assert_non_null(handle);
    assert_string_equal(handle->uri, "/tmp");

    free(handle->method_handle);

    csync_vio_handle_destroy(handle);
}

static void check_csync_vio_handle_new_null(void **state)
{
    int *number;
    csync_vio_handle_t *handle;

    (void) state; /* unused */

    number = malloc(sizeof(int));
    *number = 42;

    handle = csync_vio_handle_new(NULL, (csync_vio_method_handle_t *) number);
    assert_null(handle);

    handle = csync_vio_handle_new((char *) "/tmp", NULL);
    assert_null(handle);

    free(number);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test(check_csync_vio_handle_new),
        unit_test(check_csync_vio_handle_new_null),
    };

    return run_tests(tests);
}

