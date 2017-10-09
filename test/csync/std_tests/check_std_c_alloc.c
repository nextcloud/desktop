/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
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

#include "std/c_alloc.h"

struct test_s {
  int answer;
};

static void check_c_malloc(void **state)
{
  struct test_s *p = NULL;

  (void) state; /* unused */

  p = c_malloc(sizeof(struct test_s));
  assert_non_null(p);
  assert_int_equal(p->answer, 0);
  p->answer = 42;
  assert_int_equal(p->answer, 42);
  free(p);
}

static void check_c_malloc_zero(void **state)
{
  void *p;

  (void) state; /* unused */

  p = c_malloc((size_t) 0);
  assert_null(p);
}

static void check_c_strdup(void **state)
{
  const char *str = "test";
  char *tdup = NULL;

  (void) state; /* unused */

  tdup = c_strdup(str);
  assert_string_equal(tdup, str);

  free(tdup);
}

static void check_c_strndup(void **state)
{
  const char *str = "test";
  char *tdup = NULL;

  (void) state; /* unused */

  tdup = c_strndup(str, 3);
  assert_memory_equal(tdup, "tes", 3);

  free(tdup);
}

int torture_run_tests(void)
{
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(check_c_malloc),
      cmocka_unit_test(check_c_malloc_zero),
      cmocka_unit_test(check_c_strdup),
      cmocka_unit_test(check_c_strndup),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}

