#define _GNU_SOURCE /* asprintf */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "support.h"

#include "csync_private.h"
#include "vio/csync_vio_local.h"

#define CSYNC_TEST_DIR "/tmp/csync/"
#define CSYNC_TEST_FILE "/tmp/csync/file.txt"

CSYNC *csync;

static void setup(void) {
  system("rm -rf /tmp/csync/");
}

static void setup_dir(void) {
  system("rm -rf /tmp/csync/");
  mkdir(CSYNC_TEST_DIR, 0755);
}

static void setup_file(void) {
  system("rm -rf /tmp/csync/");
  mkdir(CSYNC_TEST_DIR, 0755);
  system("echo \"This is a test\" > /tmp/csync/file.txt");
}

static void teardown_dir(void) {
  system("rm -rf /tmp/csync/");
}

/*
 * Test directory function
 */

START_TEST (check_csync_vio_local_mkdir)
{
  struct stat sb;
  fail_unless(csync_vio_local_mkdir(CSYNC_TEST_DIR, 0755) == 0, NULL);

  fail_unless(lstat(CSYNC_TEST_DIR, &sb) == 0, NULL);

  rmdir(CSYNC_TEST_DIR);
}
END_TEST

START_TEST (check_csync_vio_local_rmdir)
{
  struct stat sb;
  fail_unless(csync_vio_local_mkdir(CSYNC_TEST_DIR, 0755) == 0, NULL);

  fail_unless(lstat(CSYNC_TEST_DIR, &sb) == 0, NULL);

  fail_unless(csync_vio_local_rmdir(CSYNC_TEST_DIR) == 0, NULL);

  fail_unless(lstat(CSYNC_TEST_DIR, &sb) < 0, NULL);
}
END_TEST

START_TEST (check_csync_vio_local_opendir)
{
  csync_vio_method_handle_t *dh = NULL;

  dh = csync_vio_local_opendir(CSYNC_TEST_DIR);
  fail_if(dh == NULL, NULL);

  fail_unless(csync_vio_local_closedir(dh) == 0, NULL);
}
END_TEST

START_TEST (check_csync_vio_local_opendir_perm)
{
  csync_vio_method_handle_t *dh = NULL;

  fail_unless(mkdir(CSYNC_TEST_DIR, 0200) == 0, NULL);

  dh = csync_vio_local_opendir(CSYNC_TEST_DIR);
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

  dh = csync_vio_local_opendir(CSYNC_TEST_DIR);
  fail_if(dh == NULL, NULL);

  dirent = csync_vio_local_readdir(dh);
  fail_if(dirent == NULL, NULL);

  fail_unless(csync_vio_local_closedir(dh) == 0, NULL);
}
END_TEST

/*
 * Test file functions (open, read, write, close ...)
 */

START_TEST (check_csync_vio_local_creat_close)
{
  csync_vio_method_handle_t *fh = NULL;

  fh = csync_vio_local_creat(CSYNC_TEST_FILE, 0644);
  fail_if(fh == NULL, NULL);

  fail_unless(csync_vio_local_close(fh) == 0, NULL);
}
END_TEST

START_TEST (check_csync_vio_local_open_close)
{
  csync_vio_method_handle_t *fh = NULL;

  fh = csync_vio_local_open(CSYNC_TEST_FILE, O_RDONLY, 0644);
  fail_if(fh == NULL, NULL);

  fail_unless(csync_vio_local_close(fh) == 0, NULL);
}
END_TEST

START_TEST (check_csync_vio_local_read_null)
{
  char test[16] = {0};

  fail_unless(csync_vio_local_read(NULL, test, 10) == (ssize_t) -1, NULL);
}
END_TEST

START_TEST (check_csync_vio_local_read)
{
  csync_vio_method_handle_t *fh = NULL;
  char test[16] = {0};

  fh = csync_vio_local_open(CSYNC_TEST_FILE, O_RDONLY, 0644);
  fail_if(fh == NULL, NULL);

  fail_unless(csync_vio_local_read(fh, test, 14) == 14, NULL);

  fail_unless(strcmp(test, "This is a test") == 0, NULL);

  fail_unless(csync_vio_local_close(fh) == 0, NULL);
}
END_TEST

START_TEST (check_csync_vio_local_read_0)
{
  csync_vio_method_handle_t *fh = NULL;
  char test[16] = {0};

  fh = csync_vio_local_open(CSYNC_TEST_FILE, O_RDONLY, 0644);
  fail_if(fh == NULL, NULL);

  fail_unless(csync_vio_local_read(fh, test, 0) == 0, NULL);

  fail_unless(test[0] == '\0', NULL);

  fail_unless(csync_vio_local_close(fh) == 0, NULL);
}
END_TEST

START_TEST (check_csync_vio_local_write_null)
{
  char test[16] = {0};

  fail_unless(csync_vio_local_write(NULL, test, 10) == (ssize_t) -1, NULL);
}
END_TEST

START_TEST (check_csync_vio_local_write)
{
  csync_vio_method_handle_t *fh = NULL;
  char str[] = "This is a test";
  char test[16] = {0};

  fh = csync_vio_local_creat(CSYNC_TEST_FILE, 0644);
  fail_if(fh == NULL, NULL);

  fail_unless(csync_vio_local_write(fh, str, sizeof(str)) == sizeof(str), NULL);
  fail_unless(csync_vio_local_close(fh) == 0, NULL);

  fh = csync_vio_local_open(CSYNC_TEST_FILE, O_RDONLY, 0644);
  fail_if(fh == NULL, NULL);

  fail_unless(csync_vio_local_read(fh, test, 14) == 14, NULL);
  fail_unless(strcmp(test, "This is a test") == 0, NULL);

  fail_unless(csync_vio_local_close(fh) == 0, NULL);
}
END_TEST

START_TEST (check_csync_vio_local_lseek)
{
  csync_vio_method_handle_t *fh = NULL;
  char test[16] = {0};

  fh = csync_vio_local_open(CSYNC_TEST_FILE, O_RDONLY, 0644);
  fail_if(fh == NULL, NULL);

  fail_unless(csync_vio_local_lseek(fh, 10, SEEK_SET) == 10, NULL);

  fail_unless(csync_vio_local_read(fh, test, 4) == 4, NULL);
  fail_unless(strcmp(test, "test") == 0, NULL);
  fail_unless(csync_vio_local_close(fh) == 0, NULL);
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

  create_case_fixture(s, "check_csync_vio_local_creat_close", check_csync_vio_local_creat_close, setup_dir, teardown_dir);
  create_case_fixture(s, "check_csync_vio_local_open_close", check_csync_vio_local_open_close, setup_file, teardown_dir);
  create_case(s, "check_csync_vio_local_read_null", check_csync_vio_local_read_null);
  create_case_fixture(s, "check_csync_vio_local_read", check_csync_vio_local_read, setup_file, teardown_dir);
  create_case_fixture(s, "check_csync_vio_local_read_0", check_csync_vio_local_read_0, setup_file, teardown_dir);
  create_case(s, "check_csync_vio_local_write_null", check_csync_vio_local_write_null);
  create_case_fixture(s, "check_csync_vio_local_write", check_csync_vio_local_write, setup_dir, teardown_dir);
  create_case_fixture(s, "check_csync_vio_local_lseek", check_csync_vio_local_lseek, setup_file, teardown_dir);

  return s;
}

int main(void) {
  int nf;

  Suite *s = csync_vio_local_suite();

  SRunner *sr;
  sr = srunner_create(s);
#if 0
#endif
  srunner_set_fork_status(sr, CK_NOFORK);
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

