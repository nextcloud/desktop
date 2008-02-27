#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "support.h"

#include "std/c_file.h"
#include "csync_lock.h"

#define TEST_LOCK "/tmp/csync_lock/test"

static void setup(void) {
  system("mkdir -p /tmp/csync_lock");
}

static void teardown(void) {
  system("rm -rf /tmp/csync_lock");
}

START_TEST (check_csync_lock)
{
  fail_unless(csync_lock(TEST_LOCK) == 0, NULL);
  fail_unless(c_isfile(TEST_LOCK) == 1, NULL);
  fail_unless(csync_lock(TEST_LOCK) < 0, NULL);

  csync_lock_remove(TEST_LOCK);
  fail_unless(c_isfile(TEST_LOCK) == 0, NULL);
}
END_TEST

START_TEST (check_csync_lock_content)
{
  char buf[8] = {0};
  int  fd, pid;

  fail_unless(csync_lock(TEST_LOCK) == 0, NULL);
  fail_unless(c_isfile(TEST_LOCK) == 1, NULL);

  /* open lock file */
  fd = open(TEST_LOCK, O_RDONLY);
  fail_if(fd < 0, NULL);

  /* read content */
  pid = read(fd, buf, sizeof(buf));
  close(fd);

  fail_if(pid < 0, NULL);

  /* get pid */
  buf[sizeof(buf) - 1] = '\0';
  pid = strtol(buf, NULL, 10);

  fail_unless(pid == getpid(), NULL);

  csync_lock_remove(TEST_LOCK);
  fail_unless(c_isfile(TEST_LOCK) == 0, NULL);
}
END_TEST


static Suite *csync_suite(void) {
  Suite *s = suite_create("csync_lock");

  create_case_fixture(s, "check_csync_lock", check_csync_lock, setup, teardown);
  create_case_fixture(s, "check_csync_lock_content", check_csync_lock_content, setup, teardown);

  return s;
}

int main(void) {
  int nf;

  Suite *s = csync_suite();

  SRunner *sr;
  sr = srunner_create(s);
  /* srunner_set_fork_status(sr, CK_NOFORK); */
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

