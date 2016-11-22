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

#include "csync_update.c"

#define TESTDB "/tmp/check_csync/journal.db"

static int firstrun = 1;

static void statedb_create_metadata_table(sqlite3 *db)
{
    int rc = 0;

    if( db ) {
        const char *sql = "CREATE TABLE IF NOT EXISTS metadata("
                          "phash INTEGER(8),"
                          "pathlen INTEGER,"
                          "path VARCHAR(4096),"
                          "inode INTEGER,"
                          "uid INTEGER,"
                          "gid INTEGER,"
                          "mode INTEGER,"
                          "modtime INTEGER(8),"
                          "type INTEGER,"
                          "md5 VARCHAR(32),"
                          "fileid VARCHAR(128),"
                          "remotePerm VARCHAR(128),"
                          "filesize BIGINT,"
                          "ignoredChildrenRemote INT,"
                          "contentChecksum TEXT,"
                          "contentChecksumTypeId INTEGER,"
                          "PRIMARY KEY(phash));";

        rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
        //const char *msg = sqlite3_errmsg(db);
        assert_int_equal( rc, SQLITE_OK );
    }
}

static void statedb_insert_metadata(sqlite3 *db)
{
    int rc = 0;

    if( db ) {
        char *stmt = sqlite3_mprintf("INSERT INTO metadata"
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

        char *errmsg;
        rc = sqlite3_exec(db, stmt, NULL, NULL, &errmsg);
        sqlite3_free(stmt);
        assert_int_equal( rc, SQLITE_OK );
    }
}

static int setup(void **state)
{
    CSYNC *csync;
    int rc;

    unlink(TESTDB);
    rc = system("mkdir -p /tmp/check_csync");
    assert_int_equal(rc, 0);
    rc = system("mkdir -p /tmp/check_csync1");
    assert_int_equal(rc, 0);
    csync_create(&csync, "/tmp/check_csync1");
    csync_init(csync, TESTDB);

    /* Create a new db with metadata */
    sqlite3 *db;
    csync->statedb.file = c_strdup(TESTDB);
    rc = sqlite3_open(csync->statedb.file, &db);
    statedb_create_metadata_table(db);
    if( firstrun ) {
        statedb_insert_metadata(db);
        firstrun = 0;
    }
    sqlite3_close(db);

    rc = csync_statedb_load(csync, TESTDB, &csync->statedb.db);
    assert_int_equal(rc, 0);

    *state = csync;
    
    return 0;
}

static int setup_ftw(void **state)
{
    CSYNC *csync;
    int rc;

    rc = system("mkdir -p /tmp/check_csync");
    assert_int_equal(rc, 0);
    rc = system("mkdir -p /tmp/check_csync1");
    assert_int_equal(rc, 0);
    csync_create(&csync, "/tmp");
    csync_init(csync, TESTDB);

    sqlite3 *db = NULL;
    rc = sqlite3_open_v2(TESTDB, &db, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, NULL);
    assert_int_equal(rc, SQLITE_OK);
    statedb_create_metadata_table(db);
    rc = sqlite3_close(db);
    assert_int_equal(rc, SQLITE_OK);

    rc = csync_statedb_load(csync, TESTDB, &csync->statedb.db);
    assert_int_equal(rc, 0);

    csync->statedb.file = c_strdup( TESTDB );
    *state = csync;
    
    return 0;
}

static int teardown(void **state)
{
    CSYNC *csync = *state;
    int rc;

    unlink( csync->statedb.file);
    rc = csync_destroy(csync);
    assert_int_equal(rc, 0);

    *state = NULL;
    
    return 0;
}

static int teardown_rm(void **state) {
    int rc;

    teardown(state);

    rc = system("rm -rf /tmp/check_csync");
    assert_int_equal(rc, 0);
    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);
    
    return 0;
}

/* create a file stat, caller must free memory */
static csync_vio_file_stat_t* create_fstat(const char *name,
                                           ino_t inode,
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


    if (inode == 0) {
        fs->inode = 619070;
    } else {
        fs->inode = inode;
    }
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_INODE;


    fs->size = 157459;
    fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;



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

    fs = create_fstat("file.txt", 0, 1217597845);
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

    fs = create_fstat("file.txt", 0, 1217597845);
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

static void check_csync_detect_update_db_eval(void **state)
{
    CSYNC *csync = *state;
    csync_file_stat_t *st;
    csync_vio_file_stat_t *fs;
    int rc;

    fs = create_fstat("file.txt", 0, 42);
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


static void check_csync_detect_update_db_rename(void **state)
{
    CSYNC *csync = *state;
    // csync_file_stat_t *st;

    csync_vio_file_stat_t *fs;
    int rc = 0;

    fs = create_fstat("wurst.txt", 0, 42);
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

    fs = create_fstat("file.txt", 42000, 0);
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

static void check_csync_detect_update_null(void **state)
{
    CSYNC *csync = *state;
    csync_vio_file_stat_t *fs;
    int rc;

    fs = create_fstat("file.txt", 0, 0);
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
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(check_csync_detect_update, setup, teardown_rm),
        cmocka_unit_test_setup_teardown(check_csync_detect_update_db_none, setup, teardown),
        cmocka_unit_test_setup_teardown(check_csync_detect_update_db_eval, setup, teardown),
        cmocka_unit_test_setup_teardown(check_csync_detect_update_db_rename, setup, teardown),
        cmocka_unit_test_setup_teardown(check_csync_detect_update_db_new, setup, teardown_rm),
        cmocka_unit_test_setup_teardown(check_csync_detect_update_null, setup, teardown_rm),

        cmocka_unit_test_setup_teardown(check_csync_ftw, setup_ftw, teardown_rm),
        cmocka_unit_test_setup_teardown(check_csync_ftw_empty_uri, setup_ftw, teardown_rm),
        cmocka_unit_test_setup_teardown(check_csync_ftw_failing_fn, setup_ftw, teardown_rm),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

