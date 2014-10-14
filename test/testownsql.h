/*
 *    This software is in the public domain, furnished "as is", without technical
 *       support, and with no warranty, express or implied, as to its usefulness for
 *          any purpose.
 *          */

#ifndef MIRALL_TESTOWNSQL_H
#define MIRALL_TESTOWNSQL_H

#include <QtTest>

#include <sqlite3.h>

#include "mirall/ownsql.h"

using namespace Mirall;

namespace {

const char testdbC[] = "/tmp/testdb.sqlite";
}

class TestOwnSql : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        QFileInfo fi( testdbC );

        if( fi.exists() ) {
            QFile::remove(testdbC);
        }
        fi.refresh();
        QVERIFY(!fi.exists());
    }

    void cleanupTestCase() {
        // QFile::remove(testdbC);
    }

    void testOpenDb() {
        QFileInfo fi( testdbC );
        QVERIFY( !fi.exists() ); // must not exist
        _db.open(testdbC);
        fi.refresh();
        QVERIFY(fi.exists());

    }

    void testCreate() {
        const char *sql = "CREATE TABLE addresses ( id INTEGER, name VARCHAR(4096), "
                "address VARCHAR(4096), entered INTEGER(8), PRIMARY KEY(id));";

        SqlQuery q(_db);
        q.prepare(sql);
        QVERIFY(q.exec());
    }

    void testIsSelect() {
        SqlQuery q(_db);
        q.prepare("SELECT foo FROM bar;");
        QVERIFY( q.isSelect() );

        q.prepare("UPDATE bla SET foo = 1;");
        QVERIFY( !q.isSelect());
    }

    void testInsert() {
        const char *sql = "INSERT INTO addresses (id, name, address, entered) VALUES "
                "(1, 'Gonzo Alberto', 'Moriabata 24, Palermo', 1403100844);";
        SqlQuery q(_db);
        q.prepare(sql);
        QVERIFY(q.exec());
    }

    void testInsert2() {
        const char *sql = "INSERT INTO addresses (id, name, address, entered) VALUES "
                "(?0, ?1, ?2, ?3);";
        SqlQuery q(_db);
        q.prepare(sql);
        q.bindValue(0, 2);
        q.bindValue(1, "Brucely Lafayette");
        q.bindValue(2, "Nurderway5, New York");
        q.bindValue(3, 1403101224);
        QVERIFY(q.exec());
    }

    void testSelect() {
        const char *sql = "SELECT * FROM addresses;";

        SqlQuery q(_db);
        q.prepare(sql);

        q.exec();
        while( q.next() ) {
            qDebug() << "Name: " << q.stringValue(1);
            qDebug() << "Address: " << q.stringValue(2);
        }
    }

    void testSelect2() {
        const char *sql = "SELECT * FROM addresses WHERE id=?1";
        SqlQuery q(_db);
        q.prepare(sql);
        q.bindValue(1, 2);
        q.exec();
        if( q.next() ) {
            qDebug() << "Name:" << q.stringValue(1);
            qDebug() << "Address:" << q.stringValue(2);
        }
    }

    void testPragma() {
        const char *sql = "PRAGMA table_info(addresses)";

        SqlQuery q(_db);
        int rc = q.prepare(sql);
        qDebug() << "Pragma:" << rc;
        q.exec();
        if( q.next() ) {
            qDebug() << "P:" << q.stringValue(1);
        }
    }

private:
    SqlDatabase _db;
};

#endif
