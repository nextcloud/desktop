#include "torture.h"

#include "vio/csync_vio_file_stat.h"

static void check_csync_vio_file_stat_new(void **state)
{
    csync_vio_file_stat_t *tstat;

    (void) state; /* unused */

    tstat = csync_vio_file_stat_new();
    assert_non_null(tstat);

    csync_vio_file_stat_destroy(tstat);
}


int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test(check_csync_vio_file_stat_new),
    };

    return run_tests(tests);
}

