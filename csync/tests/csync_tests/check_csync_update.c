#include "torture.h"

#include "csync_update.c"

#define TESTDB "/tmp/check_csync/journal.db"

static void setup(void **state)
{
    CSYNC *csync;
    int rc;

    rc = system("mkdir -p /tmp/check_csync");
    assert_int_equal(rc, 0);
    rc = system("mkdir -p /tmp/check_csync1");
    assert_int_equal(rc, 0);
    rc = system("mkdir -p /tmp/check_csync2");
    assert_int_equal(rc, 0);
    rc = csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2");
    assert_int_equal(rc, 0);
    rc = csync_set_config_dir(csync, "/tmp/check_csync");
    assert_int_equal(rc, 0);
    rc = csync_init(csync);
    assert_int_equal(rc, 0);
    rc = csync_statedb_load(csync, TESTDB, &csync->statedb.db);
    assert_int_equal(rc, 0);


    *state = csync;
}

static void setup_ftw(void **state)
{
    CSYNC *csync;
    int rc;

    rc = system("mkdir -p /tmp/check_csync");
    assert_int_equal(rc, 0);
    rc = system("mkdir -p /tmp/check_csync1");
    assert_int_equal(rc, 0);
    rc = system("mkdir -p /tmp/check_csync2");
    assert_int_equal(rc, 0);
    rc = csync_create(&csync, "/tmp", "/tmp");
    assert_int_equal(rc, 0);
    rc = csync_set_config_dir(csync, "/tmp/check_csync");
    assert_int_equal(rc, 0);
    rc = csync_init(csync);
    assert_int_equal(rc, 0);
    rc = csync_statedb_load(csync, TESTDB, &csync->statedb.db);
    assert_int_equal(rc, 0);

    *state = csync;
}

static void teardown(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_destroy(csync);
    assert_int_equal(rc, 0);

    *state = NULL;
}

static void teardown_rm(void **state) {
    int rc;

    teardown(state);

    rc = system("rm -rf /tmp/check_csync");
    assert_int_equal(rc, 0);
    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);
    rc = system("rm -rf /tmp/check_csync2");
    assert_int_equal(rc, 0);
}

/* create a file stat, caller must free memory */
static csync_vio_file_stat_t* create_fstat(const char *name,
                                           ino_t inode,
                                           nlink_t nlink,
                                           time_t mtime)
{
    csync_vio_file_stat_t *fs = NULL;
    time_t t;

    fs = csync_vio_file_stat_new();
    if (fs == NULL) {
        return NULL;
    }

    if (name && *name) {
        fs->name = c_strdup(name);
    } else {
        fs->name = c_strdup("file.txt");
    }

    if (fs->name == NULL) {
        csync_vio_file_stat_destroy(fs);
        return NULL;
    }

    fs->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

    fs->type = CSYNC_VIO_FILE_TYPE_REGULAR;
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;

    fs->mode = 0644;
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_PERMISSIONS;

    if (inode == 0) {
        fs->inode = 619070;
    } else {
        fs->inode = inode;
    }
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_INODE;

    fs->device = 0;

    fs->size = 157459;
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;

    if (nlink == 0) {
        fs->nlink = 1;
    } else {
        fs->nlink = nlink;
    }
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_LINK_COUNT;

    fs->uid = 1000;
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_UID;

    fs->gid = 1000;
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_GID;

    fs->blkcount = 312;
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_COUNT;

    fs->blksize = 4096;
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_SIZE;

    if (mtime == 0) {
        fs->atime = fs->ctime = fs->mtime = time(&t);
    } else {
        fs->atime = fs->ctime = fs->mtime = mtime;
    }
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ATIME;
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_CTIME;
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

    return fs;
}

static int failing_fn(CSYNC *ctx,
                      const char *file,
                      const csync_vio_file_stat_t *fs,
                      enum csync_ftw_flags_e flag)
{
  (void) ctx;
  (void) file;
  (void) fs;
  (void) flag;

  return -1;
}

/* detect a new file */
static void check_csync_detect_update(void **state)
{
    CSYNC *csync = *state;
    csync_file_stat_t *st;
    csync_vio_file_stat_t *fs;
    int rc;

    fs = create_fstat("file.txt", 0, 1, 1217597845);
    assert_non_null(fs);

    rc = _csync_detect_update(csync,
                              "/tmp/check_csync1/file.txt",
                              fs,
                              CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, 0);

    /* the instruction should be set to new  */
    st = c_rbtree_node_data(csync->local.tree->root);
    assert_int_equal(st->instruction, CSYNC_INSTRUCTION_NEW);

    /* create a statedb */
    csync_set_status(csync, 0xFFFF);

    csync_vio_file_stat_destroy(fs);
}

/* Test behaviour in case no db is there. For that its important that the
 * test before this one uses teardown_rm.
 */
static void check_csync_detect_update_db_none(void **state)
{
    CSYNC *csync = *state;
    csync_file_stat_t *st;
    csync_vio_file_stat_t *fs;
    int rc;

    fs = create_fstat("file.txt", 0, 1, 1217597845);
    assert_non_null(fs);

    rc = _csync_detect_update(csync,
                              "/tmp/check_csync1/file.txt",
                              fs,
                              CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, 0);

    /* the instruction should be set to new  */
    st = c_rbtree_node_data(csync->local.tree->root);
    assert_int_equal(st->instruction, CSYNC_INSTRUCTION_NEW);

    /* set the instruction to UPDATED that it gets written to the statedb */
    st->instruction = CSYNC_INSTRUCTION_UPDATED;

    /* create a statedb */
    csync_set_status(csync, 0xFFFF);

    csync_vio_file_stat_destroy(fs);
}

static void check_csync_detect_update_db_eval(void **state)
{
    CSYNC *csync = *state;
    csync_file_stat_t *st;
    csync_vio_file_stat_t *fs;
    int rc;

    fs = create_fstat("file.txt", 0, 1, 42);
    assert_non_null(fs);

    rc = _csync_detect_update(csync,
                              "/tmp/check_csync1/file.txt",
                              fs,
                              CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, 0);

    /* the instruction should be set to new  */
    st = c_rbtree_node_data(csync->local.tree->root);
    assert_int_equal(st->instruction, CSYNC_INSTRUCTION_NEW);

    /* set the instruction to UPDATED that it gets written to the statedb */
    st->instruction = CSYNC_INSTRUCTION_UPDATED;

    /* create a statedb */
    csync_set_status(csync, 0xFFFF);

    csync_vio_file_stat_destroy(fs);
}

static void check_csync_detect_update_db_rename(void **state)
{
    CSYNC *csync = *state;
    // csync_file_stat_t *st;

    csync_vio_file_stat_t *fs;
    int rc = 0;
    char *stmt = NULL;

    // rc = csync_statedb_create_tables(csync->statedb.db);

    assert_int_equal(rc, 0);
    stmt = sqlite3_mprintf("INSERT INTO metadata"
                           "(phash, pathlen, path, inode, uid, gid, mode, modtime,type,md5) VALUES"
                           "(%lld, %d, '%q', %d, %d, %d, %d, %lld, %d, '%q');",
                           (long long signed int)42,
                           42,
                           "I_was_wurst_before_I_became_wurstsalat",
                           619070,
                           42,
                           42,
                           42,
                           (long long signed int)42,
                           0,
                           "4711");

    // rc = csync_statedb_insert(csync->statedb.db, stmt);
    sqlite3_free(stmt);

    fs = create_fstat("wurst.txt", 0, 1, 42);
    assert_non_null(fs);
    csync_set_statedb_exists(csync, 1);

    rc = _csync_detect_update(csync,
                              "/tmp/check_csync1/wurst.txt",
                              fs,
                              CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, 0);

    /* the instruction should be set to rename */
    /*
     * temporarily broken.
    st = c_rbtree_node_data(csync->local.tree->root);
    assert_int_equal(st->instruction, CSYNC_INSTRUCTION_RENAME);

    st->instruction = CSYNC_INSTRUCTION_UPDATED;
    */
    /* create a statedb */
    csync_set_status(csync, 0xFFFF);

    csync_vio_file_stat_destroy(fs);
}

static void check_csync_detect_update_db_new(void **state)
{
    CSYNC *csync = *state;
    csync_file_stat_t *st;
    csync_vio_file_stat_t *fs;
    int rc;

    fs = create_fstat("file.txt", 42000, 1, 0);
    assert_non_null(fs);

    rc = _csync_detect_update(csync,
                              "/tmp/check_csync1/file.txt",
                              fs,
                              CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, 0);

    /* the instruction should be set to new  */
    st = c_rbtree_node_data(csync->local.tree->root);
    assert_int_equal(st->instruction, CSYNC_INSTRUCTION_NEW);

    /* set the instruction to UPDATED that it gets written to the statedb */
    st->instruction = CSYNC_INSTRUCTION_UPDATED;

    /* create a statedb */
    csync_set_status(csync, 0xFFFF);

    csync_vio_file_stat_destroy(fs);
}

static void check_csync_detect_update_nlink(void **state)
{
    CSYNC *csync = *state;
    csync_file_stat_t *st;
    csync_vio_file_stat_t *fs;
    int rc;

    /* create vio file stat with nlink greater than 1 */
    fs = create_fstat("file.txt", 0, 7, 0);
    assert_non_null(fs);

    /* add it to local tree */
    rc = _csync_detect_update(csync,
                              "/tmp/check_csync1/file.txt",
                              fs,
                              CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, 0);

    /* the instruction should be set to ignore */
    st = c_rbtree_node_data(csync->local.tree->root);
    assert_int_equal(st->instruction, CSYNC_INSTRUCTION_IGNORE);

    csync_vio_file_stat_destroy(fs);
}

static void check_csync_detect_update_null(void **state)
{
    CSYNC *csync = *state;
    csync_vio_file_stat_t *fs;
    int rc;

    fs = create_fstat("file.txt", 0, 1, 0);
    assert_non_null(fs);

    rc = _csync_detect_update(csync,
                              NULL,
                              fs,
                              CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, -1);

    rc = _csync_detect_update(csync,
                              "/tmp/check_csync1/file.txt",
                              NULL,
                              CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, -1);

    csync_vio_file_stat_destroy(fs);
}

static void check_csync_ftw(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_ftw(csync, "/tmp", csync_walker, MAX_DEPTH);
    assert_int_equal(rc, 0);
}

static void check_csync_ftw_empty_uri(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_ftw(csync, "", csync_walker, MAX_DEPTH);
    assert_int_equal(rc, -1);
}

static void check_csync_ftw_failing_fn(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_ftw(csync, "/tmp", failing_fn, MAX_DEPTH);
    assert_int_equal(rc, -1);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_csync_detect_update, setup, teardown_rm),
        unit_test_setup_teardown(check_csync_detect_update_db_none, setup, teardown),
        unit_test_setup_teardown(check_csync_detect_update_db_eval, setup, teardown),
        unit_test_setup_teardown(check_csync_detect_update_db_rename, setup, teardown),
        unit_test_setup_teardown(check_csync_detect_update_db_new, setup, teardown_rm),
        unit_test_setup_teardown(check_csync_detect_update_nlink, setup, teardown_rm),
        unit_test_setup_teardown(check_csync_detect_update_null, setup, teardown_rm),

        unit_test_setup_teardown(check_csync_ftw, setup_ftw, teardown_rm),
        unit_test_setup_teardown(check_csync_ftw_empty_uri, setup_ftw, teardown_rm),
        unit_test_setup_teardown(check_csync_ftw_failing_fn, setup_ftw, teardown_rm),
    };

    return run_tests(tests);
}

