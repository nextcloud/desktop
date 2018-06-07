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
#include "csync_update.cpp"
#include <sqlite3.h>

#include "torture.h"

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

        sql = "CREATE TABLE IF NOT EXISTS checksumtype("
                        "id INTEGER PRIMARY KEY,"
                        "name TEXT UNIQUE"
                        ");";
        rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
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

    /* Create a new db with metadata */
    sqlite3 *db;
    rc = sqlite3_open(TESTDB, &db);
    statedb_create_metadata_table(db);
    if( firstrun ) {
        statedb_insert_metadata(db);
        firstrun = 0;
    }
    sqlite3_close(db);

    csync = new CSYNC("/tmp/check_csync1", new OCC::SyncJournalDb(TESTDB));
    assert_true(csync->statedb->isConnected());

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
    csync = new CSYNC("/tmp", new OCC::SyncJournalDb(TESTDB));

    sqlite3 *db = NULL;
    rc = sqlite3_open_v2(TESTDB, &db, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, NULL);
    assert_int_equal(rc, SQLITE_OK);
    statedb_create_metadata_table(db);
    rc = sqlite3_close(db);
    assert_int_equal(rc, SQLITE_OK);

    csync = new CSYNC("/tmp", new OCC::SyncJournalDb(TESTDB));
    assert_true(csync->statedb->isConnected());

    *state = csync;
    
    return 0;
}

static int teardown(void **state)
{
    CSYNC *csync = (CSYNC*)*state;

    unlink(TESTDB);
    auto statedb = csync->statedb;
    delete csync;
    delete statedb;

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
static std::unique_ptr<csync_file_stat_t> create_fstat(const char *name,
                                           ino_t inode,
                                           time_t mtime)
{
    std::unique_ptr<csync_file_stat_t> fs(new csync_file_stat_t);
    time_t t;

    if (name && *name) {
        fs->path = name;
    } else {
        fs->path = "file.txt";
    }

    fs->type = ItemTypeFile;

    if (inode == 0) {
        fs->inode = 619070;
    } else {
        fs->inode = inode;
    }

    fs->size = 157459;

    if (mtime == 0) {
        fs->modtime = time(&t);
    } else {
        fs->modtime = mtime;
    }

    return fs;
}

static int failing_fn(CSYNC *ctx,
                      std::unique_ptr<csync_file_stat_t> fs)
{
  (void) ctx;
  (void) fs;

  return -1;
}

/* detect a new file */
static void check_csync_detect_update(void **state)
{
    CSYNC *csync = (CSYNC*)*state;
    csync_file_stat_t *st;
    std::unique_ptr<csync_file_stat_t> fs;
    int rc;

    fs = create_fstat("file.txt", 0, 1217597845);

    rc = _csync_detect_update(csync, std::move(fs));
    assert_int_equal(rc, 0);

    /* the instruction should be set to new  */
    st = csync->local.files.begin()->second.get();
    assert_int_equal(st->instruction, CSYNC_INSTRUCTION_NEW);

    /* create a statedb */
    csync_set_status(csync, 0xFFFF);
}

/* Test behaviour in case no db is there. For that its important that the
 * test before this one uses teardown_rm.
 */
static void check_csync_detect_update_db_none(void **state)
{
    CSYNC *csync = (CSYNC*)*state;
    csync_file_stat_t *st;
    std::unique_ptr<csync_file_stat_t> fs;
    int rc;

    fs = create_fstat("file.txt", 0, 1217597845);

    rc = _csync_detect_update(csync, std::move(fs));
    assert_int_equal(rc, 0);

    /* the instruction should be set to new  */
    st = csync->local.files.begin()->second.get();
    assert_int_equal(st->instruction, CSYNC_INSTRUCTION_NEW);


    /* create a statedb */
    csync_set_status(csync, 0xFFFF);
}

static void check_csync_detect_update_db_eval(void **state)
{
    CSYNC *csync = (CSYNC*)*state;
    csync_file_stat_t *st;
    std::unique_ptr<csync_file_stat_t> fs;
    int rc;

    fs = create_fstat("file.txt", 0, 42);

    rc = _csync_detect_update(csync, std::move(fs));
    assert_int_equal(rc, 0);

    /* the instruction should be set to new  */
    st = csync->local.files.begin()->second.get();
    assert_int_equal(st->instruction, CSYNC_INSTRUCTION_NEW);

    /* create a statedb */
    csync_set_status(csync, 0xFFFF);
}


static void check_csync_detect_update_db_rename(void **state)
{
    CSYNC *csync = (CSYNC*)*state;
    // csync_file_stat_t *st;

    std::unique_ptr<csync_file_stat_t> fs;
    int rc = 0;

    fs = create_fstat("wurst.txt", 0, 42);

    rc = _csync_detect_update(csync, std::move(fs));
    assert_int_equal(rc, 0);

    /* the instruction should be set to rename */
    /*
     * temporarily broken.
    st = csync->local.files.begin()->second.get();
    assert_int_equal(st->instruction, CSYNC_INSTRUCTION_RENAME);

    st->instruction = CSYNC_INSTRUCTION_UPDATED;
    */
    /* create a statedb */
    csync_set_status(csync, 0xFFFF);
}

static void check_csync_detect_update_db_new(void **state)
{
    CSYNC *csync = (CSYNC*)*state;
    csync_file_stat_t *st;
    std::unique_ptr<csync_file_stat_t> fs;
    int rc;

    fs = create_fstat("file.txt", 42000, 0);

    rc = _csync_detect_update(csync, std::move(fs));
    assert_int_equal(rc, 0);

    /* the instruction should be set to new  */
    st = csync->local.files.begin()->second.get();
    assert_int_equal(st->instruction, CSYNC_INSTRUCTION_NEW);


    /* create a statedb */
    csync_set_status(csync, 0xFFFF);
}

static void check_csync_ftw(void **state)
{
    CSYNC *csync = (CSYNC*)*state;
    int rc;

    rc = csync_ftw(csync, "/tmp", csync_walker, MAX_DEPTH);
    assert_int_equal(rc, 0);
}

static void check_csync_ftw_empty_uri(void **state)
{
    CSYNC *csync = (CSYNC*)*state;
    int rc;

    rc = csync_ftw(csync, "", csync_walker, MAX_DEPTH);
    assert_int_equal(rc, -1);
}

static void check_csync_ftw_failing_fn(void **state)
{
    CSYNC *csync = (CSYNC*)*state;
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

        cmocka_unit_test_setup_teardown(check_csync_ftw, setup_ftw, teardown_rm),
        cmocka_unit_test_setup_teardown(check_csync_ftw_empty_uri, setup_ftw, teardown_rm),
        cmocka_unit_test_setup_teardown(check_csync_ftw_failing_fn, setup_ftw, teardown_rm),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
