/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

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

        QString db = QCoreApplication::applicationDirPath() + "/test_journal.db";
        _ctx.statedb.file = c_strdup(db.toLocal8Bit());

        rc = csync_statedb_load((CSYNC*)(&_ctx), _ctx.statedb.file, &(_ctx.statedb.db));
        QVERIFY(rc == 0);
    }

    void testFullResult() {
        std::unique_ptr<csync_file_stat_t> st = csync_statedb_get_stat_by_hash((CSYNC*)(&_ctx), 2081025720555645157 );
        QVERIFY(st.get());
        QCOMPARE( QString::number(st->phash), QString::number(2081025720555645157) );
        QCOMPARE( QString::fromUtf8(st->path), QLatin1String("test2/zu/zuzu") );
        QCOMPARE( QString::number(st->inode), QString::number(1709554));
        QCOMPARE( QString::number(st->modtime), QString::number(1384415006));
        QCOMPARE( QString::number(st->type), QString::number(2));
        QCOMPARE( QString::fromUtf8(st->etag), QLatin1String("52847f2090665"));
        QCOMPARE( QString::fromUtf8(st->file_id), QLatin1String("00000557525d5af3d9625"));

    }

    void testByHash() {
        std::unique_ptr<csync_file_stat_t> st = csync_statedb_get_stat_by_hash((CSYNC*)(&_ctx), -7147279406142960289);
        QVERIFY(st.get());
        QCOMPARE(QString::fromUtf8(st->path), QLatin1String("documents/c1"));

        st = csync_statedb_get_stat_by_hash((CSYNC*)(&_ctx), 5426481156826978940);
        QVERIFY(st.get());
        QCOMPARE(QString::fromUtf8(st->path), QLatin1String("documents/c1/c2"));
    }

    void testByInode() {
        std::unique_ptr<csync_file_stat_t> st = csync_statedb_get_stat_by_inode((CSYNC*)(&_ctx), 1709555);
        QVERIFY(st.get());
        QCOMPARE(QString::fromUtf8(st->path), QLatin1String("test2/zu/zuzu/zuzuzu"));

        st = csync_statedb_get_stat_by_inode((CSYNC*)(&_ctx), 1706571);
        QVERIFY(st.get());
        QCOMPARE(QString::fromUtf8(st->path), QLatin1String("Shared/for_kf/a2"));
    }

    void testByFileId() {
        std::unique_ptr<csync_file_stat_t> st = csync_statedb_get_stat_by_file_id((CSYNC*)(&_ctx), "00000556525d5af3d9625");
        QVERIFY(st.get());
        QCOMPARE(QString::fromUtf8(st->path), QLatin1String("test2/zu"));

        st = csync_statedb_get_stat_by_file_id((CSYNC*)(&_ctx), "-0000001525d5af3d9625");
        QVERIFY(st.get());
        QCOMPARE(QString::fromUtf8(st->path), QLatin1String("Shared"));
    }

    void cleanupTestCase() {
        SAFE_FREE(_ctx.statedb.file);
        csync_statedb_close((CSYNC*)(&_ctx));
    }

};

QTEST_GUILESS_MAIN(TestCSyncSqlite)
#include "testcsyncsqlite.moc"
