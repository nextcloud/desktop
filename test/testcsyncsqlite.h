/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#ifndef MIRALL_TESTCSYNCSQLITE_H
#define MIRALL_TESTCSYNCSQLITE_H

#include "csync_statedb.h"
#include "csync_private.h"
#include <QtTest>


class TestCSyncSqlite : public QObject
{
    Q_OBJECT

private:
    CSYNC _ctx;
private slots:
    void initTestCase() {
        int rc;

        memset(&_ctx, 0, sizeof(CSYNC));

        _ctx.statedb.file = c_strdup("./test_journal.db");

        rc = csync_statedb_load((CSYNC*)(&_ctx), _ctx.statedb.file, &(_ctx.statedb.db));
        Q_ASSERT(rc == 0);
    }

    void testFullResult() {
        csync_file_stat_t *st = csync_statedb_get_stat_by_hash((CSYNC*)(&_ctx), 2081025720555645157 );
        QVERIFY(st);
        QCOMPARE( QString::number(st->phash), QString::number(2081025720555645157) );
        QCOMPARE( QString::number(st->pathlen), QString::number(13));
        QCOMPARE( QString::fromUtf8(st->path), QLatin1String("test2/zu/zuzu") );
        QCOMPARE( QString::number(st->inode), QString::number(1709554));
        QCOMPARE( QString::number(st->mode), QString::number(0));
        QCOMPARE( QString::number(st->modtime), QString::number(1384415006));
        QCOMPARE( QString::number(st->type), QString::number(2));
        QCOMPARE( QString::fromUtf8(st->etag), QLatin1String("52847f2090665"));
        QCOMPARE( QString::fromUtf8(st->file_id), QLatin1String("00000557525d5af3d9625"));

    }

    void testByHash() {
        csync_file_stat_t *st = csync_statedb_get_stat_by_hash((CSYNC*)(&_ctx), -7147279406142960289);
        QVERIFY(st);
        QCOMPARE(QString::fromUtf8(st->path), QLatin1String("documents/c1"));
        csync_file_stat_free(st);

        st = csync_statedb_get_stat_by_hash((CSYNC*)(&_ctx), 5426481156826978940);
        QVERIFY(st);
        QCOMPARE(QString::fromUtf8(st->path), QLatin1String("documents/c1/c2"));
        csync_file_stat_free(st);
    }

    void testByInode() {
        csync_file_stat_t *st = csync_statedb_get_stat_by_inode((CSYNC*)(&_ctx), 1709555);
        QVERIFY(st);
        QCOMPARE(QString::fromUtf8(st->path), QLatin1String("test2/zu/zuzu/zuzuzu"));
        csync_file_stat_free(st);

        st = csync_statedb_get_stat_by_inode((CSYNC*)(&_ctx), 1706571);
        QVERIFY(st);
        QCOMPARE(QString::fromUtf8(st->path), QLatin1String("Shared/for_kf/a2"));
        csync_file_stat_free(st);
    }

    void testByFileId() {
        csync_file_stat_t *st = csync_statedb_get_stat_by_file_id((CSYNC*)(&_ctx), "00000556525d5af3d9625");
        QVERIFY(st);
        QCOMPARE(QString::fromUtf8(st->path), QLatin1String("test2/zu"));
        csync_file_stat_free(st);

        st = csync_statedb_get_stat_by_file_id((CSYNC*)(&_ctx), "-0000001525d5af3d9625");
        QVERIFY(st);
        QCOMPARE(QString::fromUtf8(st->path), QLatin1String("Shared"));
        csync_file_stat_free(st);
    }

    void testEtag() {
        char *etag = csync_statedb_get_etag((CSYNC*)(&_ctx), 7145399680328529363 );
        QCOMPARE( QString::fromUtf8(etag), QLatin1String("52847f208be09"));
        SAFE_FREE(etag);

        etag = csync_statedb_get_etag((CSYNC*)(&_ctx), -8148768149813301136);
        QCOMPARE( QString::fromUtf8(etag), QLatin1String("530d148493894"));
        SAFE_FREE(etag);
    }

    void cleanupTestCase() {
        SAFE_FREE(_ctx.statedb.file);
        csync_statedb_close((CSYNC*)(&_ctx));
    }

};

#endif
