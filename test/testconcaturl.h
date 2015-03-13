/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#ifndef MIRALL_TESTCONCATURL_H
#define MIRALL_TESTCONCATURL_H

#include <QtTest>

#include <QUrl>
#include <QString>

#include "account.h"

using namespace OCC;

typedef QList< QPair<QString,QString> > QueryItems;

Q_DECLARE_METATYPE(QueryItems)

static QueryItems make()
{
    return QueryItems();
}

static QueryItems make(QString key, QString value)
{
    QueryItems q;
    q.append(qMakePair(key, value));
    return q;
}

static QueryItems make(QString key1, QString value1,
                       QString key2, QString value2)
{
    QueryItems q;
    q.append(qMakePair(key1, value1));
    q.append(qMakePair(key2, value2));
    return q;
}

class TestConcatUrl: public QObject
{
    Q_OBJECT
private slots:
    void testFolder()
    {
        QFETCH(QString, base);
        QFETCH(QString, concat);
        QFETCH(QueryItems, query);
        QFETCH(QString, expected);
        QUrl baseUrl("http://example.com" + base);
        QUrl resultUrl = Account::concatUrlPath(baseUrl, concat, query);
        QString result = QString::fromUtf8(resultUrl.toEncoded());
        QString expectedFull = "http://example.com" + expected;
        QCOMPARE(result, expectedFull);
    }

    void testFolder_data()
    {
        QTest::addColumn<QString>("base");
        QTest::addColumn<QString>("concat");
        QTest::addColumn<QueryItems>("query");
        QTest::addColumn<QString>("expected");

        // Tests about slashes
        QTest::newRow("noslash1")  << "/baa"  << "foo"  << make() << "/baa/foo";
        QTest::newRow("noslash2")  << ""      << "foo"  << make() << "/foo";
        QTest::newRow("noslash3")  << "/foo"  << ""     << make() << "/foo";
        QTest::newRow("noslash4")  << ""      << ""     << make() << "";
        QTest::newRow("oneslash1") << "/bar/" << "foo"  << make() << "/bar/foo";
        QTest::newRow("oneslash2") << "/"     << "foo"  << make() << "/foo";
        QTest::newRow("oneslash3") << "/foo"  << "/"    << make() << "/foo/";
        QTest::newRow("oneslash4") << ""      << "/"    << make() << "/";
        QTest::newRow("twoslash1") << "/bar/" << "/foo" << make() << "/bar/foo";
        QTest::newRow("twoslash2") << "/"     << "/foo" << make() << "/foo";
        QTest::newRow("twoslash3") << "/foo/" << "/"    << make() << "/foo/";
        QTest::newRow("twoslash4") << "/"     << "/"    << make() << "/";

        // Tests about path encoding
        QTest::newRow("encodepath")
                << "/a f/b"
                << "/a f/c"
                << make()
                << "/a%20f/b/a%20f/c";

        // Tests about query args
        QTest::newRow("query1")
                << "/baa"
                << "/foo"
                << make("a=a", "b=b",
                        "c", "d")
                << "/baa/foo?a%3Da=b%3Db&c=d";
        QTest::newRow("query2")
                << ""
                << ""
                << make("foo", "bar")
                << "?foo=bar";
    }

};

#endif
