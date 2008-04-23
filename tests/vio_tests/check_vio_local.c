#define _GNU_SOURCE /* asprintf */
#include <string.h>
#include <errno.h>

#include "support.h"

#include "csync_private.h"
#include "vio/csync_vio_local.h"

#define CSYNC_DIR "/tmp/csync/"

CSYNC *csync;

static void setup(void) {
  system("rm -rf /tmp/csync/");
}

static void setup_dir(void) {
  system("rm -rf /tmp/csync/");
  mkdir(CSYNC_DIR, 0755);
}

static void teardown_dir(void) {
  system("rm -rf /tmp/csync/");
}

START_TEST (check_csync_vio_local_mkdir)
{
  struct stat sb;
  fail_unless(csync_vio_local_mkdir(CSYNC_DIR, 0755) == 0, NULL);

  fail_unless(lstat(CSYNC_DIR, &sb) == 0, NULL);

  rmdir(CSYNC_DIR);
}
END_TEST

START_TEST (check_csync_vio_local_rmdir)
{
  struct stat sb;
  fail_unless(csync_vio_local_mkdir(CSYNC_DIR, 0755) == 0, NULL);

  fail_unless(lstat(CSYNC_DIR, &sb) == 0, NULL);

  fail_unless(csync_vio_local_rmdir(CSYNC_DIR) == 0, NULL);

  fail_unless(lstat(CSYNC_DIR, &sb) < 0, NULL);
}
END_TEST

START_TEST (check_csync_vio_local_opendir)
{
  csync_vio_method_handle_t *dh = NULL;

  dh = csync_vio_local_opendir(CSYNC_DIR);
  fail_if(dh == NULL, NULL);

  fail_unless(csync_vio_local_closedir(dh) == 0, NULL);
}
END_TEST

START_TEST (check_csync_vio_local_opendir_perm)
{
  csync_vio_method_handle_t *dh = NULL;

  fail_unless(mkdir(CSYNC_DIR, 0200) == 0, NULL);

  dh = csync_vio_local_opendir(CSYNC_DIR);
  fail_unless(dh == NULL, NULL);
  fail_unless(errno == EACCES, NULL);
}
END_TEST

START_TEST (check_csync_vio_local_closedir_null)
{
  fail_unless(csync_vio_local_closedir(NULL) < 0, NULL);
}
END_TEST

START_TEST (check_csync_vio_local_readdir)
{
  csync_vio_method_handle_t *dh = NULL;
  csync_vio_file_stat_t *dirent = NULL;

  dh = csync_vio_local_opendir(CSYNC_DIR);
  fail_if(dh == NULL, NULL);

  dirent = csync_vio_local_readdir(dh);
  fail_if(dirent == NULL, NULL);

  fail_unless(csync_vio_local_closedir(dh) == 0, NULL);
}
END_TEST

static Suite *csync_vio_local_suite(void) {
  Suite *s = suite_create("csync_vio");

  create_case_fixture(s, "check_csync_vio_local_mkdir", check_csync_vio_local_mkdir, setup, teardown_dir);
  create_case_fixture(s, "check_csync_vio_local_rmdir", check_csync_vio_local_rmdir, setup, teardown_dir);
  create_case_fixture(s, "check_csync_vio_local_opendir", check_csync_vio_local_opendir, setup_dir, teardown_dir);
  create_case_fixture(s, "check_csync_vio_local_opendir_perm", check_csync_vio_local_opendir_perm, setup, teardown_dir);
  create_case(s, "check_csync_vio_local_closedir_null", check_csync_vio_local_closedir_null);
  create_case_fixture(s, "check_csync_vio_local_readdir", check_csync_vio_local_readdir, setup_dir, teardown_dir);

  return s;
}

int main(void) {
  int nf;

  Suite *s = csync_vio_local_suite();

  SRunner *sr;
  sr = srunner_create(s);
#if 0
  srunner_set_fork_status(sr, CK_NOFORK);
#endif
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

