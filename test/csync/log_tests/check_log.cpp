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
#include <string.h>

#include "support.h"

#include "config_csync.h"
#include "csync_log.h"

static void setup(void) {
  csync_log_init();
}

static void teardown(void) {
  csync_log_fini();
}

START_TEST (log_create)
{
  fail_unless((csync_log_init() == 0), NULL);
  fail_unless((csync_log_fini() == 0), NULL);
}
END_TEST

START_TEST (log_load)
{
  char buf[256];
  snprintf(buf, (size_t) 256 - 1, "%s/%s", SOURCEDIR, "config/ocsync_log.conf");
  fail_unless(csync_log_load(buf) == 0);
}
END_TEST

START_TEST (log_prio)
{
  CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL, "log %s", "test");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_FATAL, "log %s", "test");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_ALERT, "log %s", "test");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_CRIT, "log %s", "test");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "log %s", "test");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "log %s", "test");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "log %s", "test");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_INFO, "log %s", "test");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "log %s", "test");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "log %s", "test");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTSET, "log %s", "test");
  CSYNC_LOG(CSYNC_LOG_PRIORITY_UNKNOWN, "log %s", "test");
}
END_TEST

START_TEST (log_null)
{
  char *z = NULL;
  CSYNC_LOG(CSYNC_LOG_PRIORITY_UNKNOWN, "log %s", z);
}
END_TEST

static Suite *log_suite(void) {
  Suite *s = suite_create("Logger");

  create_case(s, "log_create", log_create);
  create_case_fixture(s, "log_load", log_load, setup, teardown);
  create_case_fixture(s, "log_prio", log_prio, setup, teardown);
  create_case_fixture(s, "log_null", log_null, setup, teardown);

  return s;
}

int main(void) {
  int nf;

  Suite *s = log_suite();

  SRunner *sr;
  sr = srunner_create(s);
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

