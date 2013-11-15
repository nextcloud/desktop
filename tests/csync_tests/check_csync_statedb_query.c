#include "torture.h"

#define CSYNC_TEST 1
#include "csync_statedb.c"

#define TESTDB "/tmp/check_csync1/test.db"
#define TESTDBTMP "/tmp/check_csync1/test.db.ctmp"



static void setup(void **state)
{
    CSYNC *csync;
    int rc = 0;

    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);
    rc = system("rm -rf /tmp/check_csync2");
    assert_int_equal(rc, 0);
    rc = system("mkdir -p /tmp/check_csync1");
    assert_int_equal(rc, 0);
    rc = system("mkdir -p /tmp/check_csync2");
    assert_int_equal(rc, 0);
    rc = system("mkdir -p /tmp/check_csync");
    assert_int_equal(rc, 0);
    rc = csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2");
    assert_int_equal(rc, 0);
    rc = csync_set_config_dir(csync, "/tmp/check_csync/");
    assert_int_equal(rc, 0);
    rc = csync_init(csync);
    assert_int_equal(rc, 0);

    rc = csync_statedb_load(csync, TESTDB, &csync->statedb.db);
    assert_int_equal(rc, 0);

    *state = csync;
}

static void setup_db(void **state)
{
    CSYNC *csync;
    char *stmt = NULL;
    int rc = 0;
    c_strlist_t *result = NULL;

    setup(state);
    csync = *state;

    // rc = csync_statedb_create_tables(csync->statedb.db);
    assert_int_equal(rc, 0);

    result = csync_statedb_query(csync->statedb.db,
        "CREATE TABLE IF NOT EXISTS metadata ("
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
        "PRIMARY KEY(phash)"
        ");"
        );

    assert_non_null(result);
    c_strlist_destroy(result);


    stmt = sqlite3_mprintf("INSERT INTO metadata"
                           "(phash, pathlen, path, inode, uid, gid, mode, modtime, type, md5) VALUES"
                           "(%lu, %d, '%q', %d, %d, %d, %d, %lu, %d, %lu);",
                           42,
                           42,
                           "It's a rainy day",
                           23,
                           42,
                           42,
                           42,
                           42,
                           2,
                           43);

    // rc = csync_statedb_insert(csync->statedb.db, stmt);
    sqlite3_free(stmt);
}

static void teardown(void **state) {
    CSYNC *csync = *state;
    int rc = 0;

    rc = csync_destroy(csync);
    assert_int_equal(rc, 0);
    rc = system("rm -rf /tmp/check_csync");
    assert_int_equal(rc, 0);
    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);
    rc = system("rm -rf /tmp/check_csync2");
    assert_int_equal(rc, 0);

    *state = NULL;
}


static void check_csync_statedb_query_statement(void **state)
{
    CSYNC *csync = *state;
    c_strlist_t *result;

    result = csync_statedb_query(csync->statedb.db, "");
    assert_null(result);
    if (result != NULL) {
      c_strlist_destroy(result);
    }

    result = csync_statedb_query(csync->statedb.db, "SELECT;");
    assert_null(result);
    if (result != NULL) {
      c_strlist_destroy(result);
    }
}

static void check_csync_statedb_create_error(void **state)
{
    CSYNC *csync = *state;
    c_strlist_t *result;

    result = csync_statedb_query(csync->statedb.db, "CREATE TABLE test(phash INTEGER, text VARCHAR(10));");
    assert_non_null(result);
    c_strlist_destroy(result);

    result = csync_statedb_query(csync->statedb.db, "CREATE TABLE test(phash INTEGER, text VARCHAR(10));");
    assert_null(result);

    c_strlist_destroy(result);
}

static void check_csync_statedb_insert_statement(void **state)
{
    CSYNC *csync = *state;
    c_strlist_t *result;
    int rc = 0;

    result = csync_statedb_query(csync->statedb.db, "CREATE TABLE test(phash INTEGER, text VARCHAR(10));");
    assert_non_null(result);
    c_strlist_destroy(result);

    // rc = csync_statedb_insert(csync->statedb.db, "INSERT;");
    assert_int_equal(rc, 0);
    // rc = csync_statedb_insert(csync->statedb.db, "INSERT");
    assert_int_equal(rc, 0);
    // rc = csync_statedb_insert(csync->statedb.db, "");
    assert_int_equal(rc, 0);
}



static void check_csync_statedb_drop_tables(void **state)
{
    // CSYNC *csync = *state;
    int rc = 0;
    (void) state;

    // rc = csync_statedb_drop_tables(csync->statedb.db);
    assert_int_equal(rc, 0);
    // rc = csync_statedb_create_tables(csync->statedb.db);
    assert_int_equal(rc, 0);
    // rc = csync_statedb_drop_tables(csync->statedb.db);
    assert_int_equal(rc, 0);
}

static void check_csync_statedb_insert_metadata(void **state)
{
    CSYNC *csync = *state;
    csync_file_stat_t *st;
    int i, rc = 0;

    // rc = csync_statedb_create_tables(csync->statedb.db);
    assert_int_equal(rc, 0);

    for (i = 0; i < 100; i++) {
        st = c_malloc(sizeof(csync_file_stat_t) + 30 );
        snprintf(st->path, 29, "file_%d" , i );
        st->phash = i;

        rc = c_rbtree_insert(csync->local.tree, (void *) st);
        assert_int_equal(rc, 0);
    }

    // rc = csync_statedb_insert_metadata(csync, csync->statedb.db);
    assert_int_equal(rc, 0);
}

static void check_csync_statedb_write(void **state)
{
    CSYNC *csync = *state;
    csync_file_stat_t *st;
    int i, rc;

    for (i = 0; i < 100; i++) {
        st = c_malloc(sizeof(csync_file_stat_t) + 30);
        snprintf(st->path, 29, "file_%d" , i );
        st->phash = i;

        rc = c_rbtree_insert(csync->local.tree, (void *) st);
        assert_int_equal(rc, 0);
    }

    // rc = csync_statedb_write(csync, csync->statedb.db);
    assert_int_equal(rc, 0);
}


static void check_csync_statedb_get_stat_by_hash_not_found(void **state)
{
    CSYNC *csync = *state;
    csync_file_stat_t *tmp;

    tmp = csync_statedb_get_stat_by_hash(csync->statedb.db, (uint64_t) 666);
    assert_null(tmp);

    free(tmp);
}

static void check_csync_statedb_get_stat_by_inode(void **state)
{
    CSYNC *csync = *state;
    csync_file_stat_t *tmp;

    tmp = csync_statedb_get_stat_by_inode(csync->statedb.db, (ino_t) 23);
    assert_non_null(tmp);

    assert_int_equal(tmp->phash, 42);
    assert_int_equal(tmp->inode, 23);

    free(tmp);
}

static void check_csync_statedb_get_stat_by_inode_not_found(void **state)
{
    CSYNC *csync = *state;
    csync_file_stat_t *tmp;

    tmp = csync_statedb_get_stat_by_inode(csync->statedb.db, (ino_t) 666);
    assert_null(tmp);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_csync_statedb_query_statement, setup, teardown),
        unit_test_setup_teardown(check_csync_statedb_create_error, setup, teardown),
        unit_test_setup_teardown(check_csync_statedb_insert_statement, setup, teardown),
      /*  unit_test_setup_teardown(check_csync_statedb_is_empty, setup, teardown), */
      /*  unit_test_setup_teardown(check_csync_statedb_create_tables, setup, teardown), */
        unit_test_setup_teardown(check_csync_statedb_drop_tables, setup, teardown),
        unit_test_setup_teardown(check_csync_statedb_insert_metadata, setup, teardown),
        unit_test_setup_teardown(check_csync_statedb_write, setup, teardown),
     /*   unit_test_setup_teardown(check_csync_statedb_get_stat_by_hash, setup_db, teardown), */
        unit_test_setup_teardown(check_csync_statedb_get_stat_by_hash_not_found, setup_db, teardown),
      /* unit_test_setup_teardown(check_csync_statedb_get_stat_by_inode, setup_db, teardown), */
        unit_test_setup_teardown(check_csync_statedb_get_stat_by_inode_not_found, setup_db, teardown),
    };

    return run_tests(tests);
}

