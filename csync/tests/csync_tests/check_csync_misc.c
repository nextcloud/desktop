#include "torture.h"

#include "csync_misc.h"
#include <stdlib.h>

static void check_csync_normalize_etag(void **state)
{
  char *str;

  (void) state; /* unused */

#define CHECK_NORMALIZE_ETAG(TEST, EXPECT) \
    str = csync_normalize_etag(TEST); \
    assert_string_equal(str, EXPECT); \
    free(str);


  CHECK_NORMALIZE_ETAG("foo", "foo");
  CHECK_NORMALIZE_ETAG("\"foo\"", "foo");
  CHECK_NORMALIZE_ETAG("\"nar123\"", "nar123");
  CHECK_NORMALIZE_ETAG("", "");
  CHECK_NORMALIZE_ETAG("\"\"", "");

  /* Test with -gzip (all combinaison) */
  CHECK_NORMALIZE_ETAG("foo-gzip", "foo");
  CHECK_NORMALIZE_ETAG("\"foo\"-gzip", "foo");
  CHECK_NORMALIZE_ETAG("\"foo-gzip\"", "foo");
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test(check_csync_normalize_etag),
    };

    return run_tests(tests);
}

