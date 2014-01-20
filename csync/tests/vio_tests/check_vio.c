#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "torture.h"

#include "csync_private.h"
#include "vio/csync_vio.h"

#define CSYNC_TEST_DIR "/tmp/csync_test/"
#define CSYNC_TEST_DIRS "/tmp/csync_test/this/is/a/mkdirs/test"
#define CSYNC_TEST_FILE "/tmp/csync_test/file.txt"

#define MKDIR_MASK (S_IRWXU |S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)

#define WD_BUFFER_SIZE 255

static char wd_buffer[WD_BUFFER_SIZE];

static void setup(void **state)
{
    CSYNC *csync;
    int rc;

    assert_non_null(getcwd(wd_buffer, WD_BUFFER_SIZE));

    rc = system("rm -rf /tmp/csync_test");
    assert_int_equal(rc, 0);

    rc = csync_create(&csync, "/tmp/csync1", "/tmp/csync2");
    assert_int_equal(rc, 0);

    csync->replica = LOCAL_REPLICA;

    *state = csync;
}

static void setup_dir(void **state) {
    int rc;
    mbchar_t *dir = c_utf8_to_locale(CSYNC_TEST_DIR);

    setup(state);

    rc = _tmkdir(dir, MKDIR_MASK);
    c_free_locale_string(dir);
    assert_int_equal(rc, 0);

    assert_non_null(getcwd(wd_buffer, WD_BUFFER_SIZE));

    rc = chdir(CSYNC_TEST_DIR);
    assert_int_equal(rc, 0);
}

static void setup_file(void **state) {
    int rc;

    setup_dir(state);

    rc = system("echo \"This is a test\" > /tmp/csync_test/file.txt");
    assert_int_equal(rc, 0);
}

static void teardown(void **state) {
    CSYNC *csync = *state;
    int rc;

    rc = csync_destroy(csync);
    assert_int_equal(rc, 0);

    rc = chdir(wd_buffer);
    assert_int_equal(rc, 0);

    rc = system("rm -rf /tmp/csync_test/");
    assert_int_equal(rc, 0);

    *state = NULL;
}


/*
 * Test directory function
 */

static void check_csync_vio_mkdir(void **state)
{
    CSYNC *csync = *state;
    csync_stat_t sb;
    int rc;
    mbchar_t *dir = c_utf8_to_locale(CSYNC_TEST_DIR);

    rc = csync_vio_mkdir(csync, CSYNC_TEST_DIR, MKDIR_MASK);
    assert_int_equal(rc, 0);

    rc = _tstat(dir, &sb);
    assert_int_equal(rc, 0);

    _trmdir(dir);
    c_free_locale_string(dir);
}

static void check_csync_vio_mkdirs(void **state)
{
    CSYNC *csync = *state;
    csync_stat_t sb;
    int rc;
    mbchar_t *dir = c_utf8_to_locale(CSYNC_TEST_DIR);

    rc = csync_vio_mkdirs(csync, CSYNC_TEST_DIRS, MKDIR_MASK);
    assert_int_equal(rc, 0);

    rc = _tstat(dir, &sb);
    assert_int_equal(rc, 0);

    _trmdir(dir);
    c_free_locale_string(dir);
}

static void check_csync_vio_mkdirs_some_exist(void **state)
{
    CSYNC *csync = *state;
    csync_stat_t sb;
    mbchar_t *this_dir = c_utf8_to_locale("/tmp/csync_test/this");
    mbchar_t *stat_dir = c_utf8_to_locale(CSYNC_TEST_DIRS);
    int rc;

    rc = _tmkdir(this_dir, MKDIR_MASK);
    assert_int_equal(rc, 0);
    rc = csync_vio_mkdirs(csync, CSYNC_TEST_DIRS, MKDIR_MASK);
    assert_int_equal(rc, 0);

    rc = _tstat(stat_dir, &sb);
    assert_int_equal(rc, 0);

    _trmdir(stat_dir);
    c_free_locale_string(this_dir);
    c_free_locale_string(stat_dir);
}

static void check_csync_vio_rmdir(void **state)
{
    CSYNC *csync = *state;
    csync_stat_t sb;
    int rc;

    rc = csync_vio_mkdir(csync, CSYNC_TEST_DIR, MKDIR_MASK);
    assert_int_equal(rc, 0);

    rc = lstat(CSYNC_TEST_DIR, &sb);
    assert_int_equal(rc, 0);

    rc = csync_vio_rmdir(csync, CSYNC_TEST_DIR);
    assert_int_equal(rc, 0);

    rc = lstat(CSYNC_TEST_DIR, &sb);
    assert_int_equal(rc, -1);
}

static void check_csync_vio_opendir(void **state)
{
    CSYNC *csync = *state;
    csync_vio_method_handle_t *dh;
    int rc;

    dh = csync_vio_opendir(csync, CSYNC_TEST_DIR);
    assert_non_null(dh);

    rc = csync_vio_closedir(csync, dh);
    assert_int_equal(rc, 0);
}

static void check_csync_vio_opendir_perm(void **state)
{
    CSYNC *csync = *state;
    csync_vio_method_handle_t *dh;
    int rc;
    mbchar_t *dir = c_utf8_to_locale(CSYNC_TEST_DIR);

    assert_non_null(dir);

    rc = _tmkdir(dir, (S_IWUSR|S_IXUSR));
    assert_int_equal(rc, 0);

    dh = csync_vio_opendir(csync, CSYNC_TEST_DIR);
    assert_null(dh);
    assert_int_equal(errno, EACCES);

    _tchmod(dir, MKDIR_MASK);
    c_free_locale_string(dir);
}

static void check_csync_vio_closedir_null(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_vio_closedir(csync, NULL);
    assert_int_equal(rc, -1);
}

static void check_csync_vio_readdir(void **state)
{
    CSYNC *csync = *state;
    csync_vio_method_handle_t *dh;
    csync_vio_file_stat_t *dirent;
    int rc;

    dh = csync_vio_opendir(csync, CSYNC_TEST_DIR);
    assert_non_null(dh);

    dirent = csync_vio_readdir(csync, dh);
    assert_non_null(dirent);

    csync_vio_file_stat_destroy(dirent);
    rc = csync_vio_closedir(csync, dh);
    assert_int_equal(rc, 0);
}

/*
 * Test file functions (open, read, write, close ...)
 */

static void check_csync_vio_close_null(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_vio_close(csync, NULL);
    assert_int_equal(rc, -1);
}

static void check_csync_vio_creat_close(void **state)
{
    CSYNC *csync = *state;
    csync_vio_method_handle_t *fh;
    int rc;

    fh = csync_vio_creat(csync, CSYNC_TEST_FILE, 0644);
    assert_non_null(fh);

    rc = csync_vio_close(csync, fh);
    assert_int_equal(rc, 0);
}

static void check_csync_vio_open_close(void **state)
{
    CSYNC *csync = *state;
    csync_vio_method_handle_t *fh;
    int rc;

    fh = csync_vio_open(csync, CSYNC_TEST_FILE, O_RDONLY, 0644);
    assert_non_null(fh);

    rc = csync_vio_close(csync, fh);
    assert_int_equal(rc, 0);
}

static void check_csync_vio_read_null(void **state)
{
    CSYNC *csync = *state;
    char test[16] = {0};
    int rc;

    rc = csync_vio_read(csync, NULL, test, 10);
    assert_int_equal(rc, -1);
}

static void check_csync_vio_read(void **state)
{
    CSYNC *csync = *state;
    csync_vio_method_handle_t *fh;
    char test[16] = {0};
    int rc;

    fh = csync_vio_open(csync, CSYNC_TEST_FILE, O_RDONLY, 0644);
    assert_non_null(fh);

    rc = csync_vio_read(csync, fh, test, 14);
    assert_int_equal(rc, 14);

    assert_string_equal(test, "This is a test");

    rc = csync_vio_close(csync, fh);
    assert_int_equal(rc, 0);
}

static void check_csync_vio_read_0(void **state)
{
    CSYNC *csync = *state;
    csync_vio_method_handle_t *fh = NULL;
    char test[16] = {0};
    int rc;

    fh = csync_vio_open(csync, CSYNC_TEST_FILE, O_RDONLY, 0644);
    assert_non_null(fh);

    rc = csync_vio_read(csync, fh, test, 0);
    assert_int_equal(rc, 0);

    assert_true(test[0] == '\0');

    rc = csync_vio_close(csync, fh);
    assert_int_equal(rc, 0);
}

static void check_csync_vio_write_null(void **state)
{
    CSYNC *csync = *state;
    char test[16] = {0};
    int rc;

    rc = csync_vio_write(csync, NULL, test, 10);
    assert_int_equal(rc, -1);
}

static void check_csync_vio_write(void **state)
{
    CSYNC *csync = *state;
    csync_vio_method_handle_t *fh;
    char str[] = "This is a test";
    char test[16] = {0};
    int rc;

    fh = csync_vio_creat(csync, CSYNC_TEST_FILE, 0644);
    assert_non_null(fh);

    rc = csync_vio_write(csync, fh, str, sizeof(str));
    assert_int_equal(rc, sizeof(str));

    rc = csync_vio_close(csync, fh);
    assert_int_equal(rc, 0);

    fh = csync_vio_open(csync, CSYNC_TEST_FILE, O_RDONLY, 0644);
    assert_non_null(fh);

    rc = csync_vio_read(csync, fh, test, 14);
    assert_int_equal(rc, 14);

    assert_string_equal(test, "This is a test");

    rc = csync_vio_close(csync, fh);
    assert_int_equal(rc, 0);
}

static void check_csync_vio_lseek(void **state)
{
    CSYNC *csync = *state;
    csync_vio_method_handle_t *fh;
    char test[16] = {0};
    int rc;

    fh = csync_vio_open(csync, CSYNC_TEST_FILE, O_RDONLY, 0644);
    assert_non_null(fh);

    rc = csync_vio_lseek(csync, fh, 10, SEEK_SET);
    assert_int_equal(rc, 10);

    rc = csync_vio_read(csync, fh, test, 4);
    assert_int_equal(rc, 4);

    assert_string_equal(test, "test");

    rc = csync_vio_close(csync, fh);
    assert_int_equal(rc, 0);
}

/*
 * Test for general functions (stat, chmod, chown, ...)
 */

static void check_csync_vio_stat_dir(void **state)
{
    CSYNC *csync = *state;
    csync_vio_file_stat_t *fs;
    int rc;

    fs = csync_vio_file_stat_new();
    assert_non_null(fs);

    rc = csync_vio_stat(csync, CSYNC_TEST_DIR, fs);
    assert_int_equal(rc, 0);

    assert_string_equal(fs->name, "csync_test");
    assert_int_equal(fs->type, CSYNC_VIO_FILE_TYPE_DIRECTORY);

    csync_vio_file_stat_destroy(fs);
}

static void check_csync_vio_stat_file(void **state)
{
    CSYNC *csync = *state;
    csync_vio_file_stat_t *fs;
    int rc;

    fs = csync_vio_file_stat_new();
    assert_non_null(fs);

    rc = csync_vio_stat(csync, CSYNC_TEST_FILE, fs);
    assert_int_equal(rc, 0);

    assert_string_equal(fs->name, "file.txt");
    assert_int_equal(fs->type, CSYNC_VIO_FILE_TYPE_REGULAR);

    csync_vio_file_stat_destroy(fs);
}

static void check_csync_vio_rename_dir(void **state)
{
    CSYNC *csync = *state;
    csync_stat_t sb;
    int rc;

    mbchar_t *dir = c_utf8_to_locale("test");
    mbchar_t *dir2 = c_utf8_to_locale("test2");

    assert_non_null(dir);
    assert_non_null(dir2);

    rc = _tmkdir(dir, MKDIR_MASK);
    assert_int_equal(rc, 0);

    rc = csync_vio_rename(csync, "test", "test2");
    assert_int_equal(rc, 0);


    rc = _tstat(dir2, &sb);
    assert_int_equal(rc, 0);

    c_free_locale_string(dir);
    c_free_locale_string(dir2);
}

static void check_csync_vio_rename_file(void **state)
{
    CSYNC *csync = *state;
    mbchar_t *file = c_utf8_to_locale(CSYNC_TEST_DIR "file2.txt");
    csync_stat_t sb;
    int rc;

    rc = csync_vio_rename(csync, CSYNC_TEST_FILE, CSYNC_TEST_DIR "file2.txt");
    assert_int_equal(rc, 0);

    rc = _tstat(file, &sb);
    assert_int_equal(rc, 0);

    c_free_locale_string(file);
}

static void check_csync_vio_unlink(void **state)
{
    CSYNC *csync = *state;
    csync_stat_t sb;
    mbchar_t *file = c_utf8_to_locale(CSYNC_TEST_FILE);
    int rc;

    rc = csync_vio_unlink(csync, CSYNC_TEST_FILE);
    assert_int_equal(rc, 0);

    rc = _tstat(file, &sb);
    assert_int_equal(rc, -1);

    c_free_locale_string(file);
}

static void check_csync_vio_chmod(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_vio_chmod(csync, CSYNC_TEST_FILE, 0777);
    assert_int_equal(rc, 0);
}

#ifndef _WIN32
static void check_csync_vio_chown(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_vio_chown(csync, CSYNC_TEST_FILE, getuid(), getgid());
    assert_int_equal(rc, 0);
}
#endif

static void check_csync_vio_utimes(void **state)
{
    CSYNC *csync = *state;
    csync_stat_t sb;
    struct timeval times[2];
    long modtime = 0;
    mbchar_t *file = c_utf8_to_locale(CSYNC_TEST_FILE);
    int rc;

    rc = _tstat(file, &sb);
    assert_int_equal(rc, 0);
    modtime = sb.st_mtime + 10;

    times[0].tv_sec = modtime;
    times[0].tv_usec = 0;

    times[1].tv_sec = modtime;
    times[1].tv_usec = 0;

    rc = csync_vio_utimes(csync, CSYNC_TEST_FILE, times);
    assert_int_equal(rc, 0);

    rc = _tstat(file, &sb);
    assert_int_equal(rc, 0);

    assert_int_equal(modtime, sb.st_mtime);

    c_free_locale_string(file);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {

        unit_test_setup_teardown(check_csync_vio_mkdir, setup, teardown),
        unit_test_setup_teardown(check_csync_vio_mkdirs, setup, teardown),
        unit_test_setup_teardown(check_csync_vio_mkdirs_some_exist, setup_dir, teardown),
        unit_test_setup_teardown(check_csync_vio_rmdir, setup, teardown),
        unit_test_setup_teardown(check_csync_vio_opendir, setup_dir, teardown),
        unit_test_setup_teardown(check_csync_vio_opendir_perm, setup, teardown),
        unit_test(check_csync_vio_closedir_null),
        unit_test_setup_teardown(check_csync_vio_readdir, setup_dir, teardown),

        unit_test_setup_teardown(check_csync_vio_close_null, setup_dir, teardown),
        unit_test_setup_teardown(check_csync_vio_creat_close, setup_dir, teardown),
        unit_test_setup_teardown(check_csync_vio_open_close, setup_file, teardown),
        unit_test(check_csync_vio_read_null),
        unit_test_setup_teardown(check_csync_vio_read, setup_file, teardown),
        unit_test_setup_teardown(check_csync_vio_read_0, setup_file, teardown),
        unit_test(check_csync_vio_write_null),
        unit_test_setup_teardown(check_csync_vio_write, setup_dir, teardown),
        unit_test_setup_teardown(check_csync_vio_lseek, setup_file, teardown),

        unit_test_setup_teardown(check_csync_vio_stat_dir, setup_dir, teardown),
        unit_test_setup_teardown(check_csync_vio_stat_file, setup_file, teardown),

        unit_test_setup_teardown(check_csync_vio_rename_dir, setup_dir, teardown),
        unit_test_setup_teardown(check_csync_vio_rename_file, setup_file, teardown),
        unit_test_setup_teardown(check_csync_vio_unlink, setup_file, teardown),
        unit_test_setup_teardown(check_csync_vio_chmod, setup_file, teardown),
#ifndef _WIN32
        unit_test_setup_teardown(check_csync_vio_chown, setup_file, teardown),
#endif
        unit_test_setup_teardown(check_csync_vio_utimes, setup_file, teardown),
    };

    return run_tests(tests);
}

