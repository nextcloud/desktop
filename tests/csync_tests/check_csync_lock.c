#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "support.h"

#define CSYNC_TEST 1
#include "std/c_file.h"
#include "csync_lock.h"

#define TEST_LOCK "/tmp/check_csync/test"

static void setup(void) {
  fail_if(system("mkdir -p /tmp/check_csync") < 0, "Setup failed");
}

static void teardown(void) {
  fail_if(system("rm -rf /tmp/check_csync") < 0, "Teardown failed");
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


static Suite *make_csync_suite(void) {
  Suite *s = suite_create("csync_lock");

  create_case_fixture(s, "check_csync_lock", check_csync_lock, setup, teardown);
  create_case_fixture(s, "check_csync_lock_content", check_csync_lock_content, setup, teardown);

  return s;
}

int main(int argc, char **argv) {
  Suite *s = NULL;
  SRunner *sr = NULL;
  struct argument_s arguments;
  int nf;

  ZERO_STRUCT(arguments);

  cmdline_parse(argc, argv, &arguments);

  s = make_csync_suite();

  sr = srunner_create(s);
  if (arguments.nofork) {
    srunner_set_fork_status(sr, CK_NOFORK);
  }
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

