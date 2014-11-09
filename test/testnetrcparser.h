/*
 *    This software is in the public domain, furnished "as is", without technical
 *       support, and with no warranty, express or implied, as to its usefulness for
 *          any purpose.
 *          */

#ifndef MIRALL_INOTIFYWATCHER_H
#define MIRALL_INOTIFYWATCHER_H

#include <QtTest>

#include "cmd/netrcparser.h"

using namespace OCC;

namespace {

const char testfileC[] = "netrctest";
const char testfileWithDefaultC[] = "netrctestDefault";
const char testfileEmptyC[] = "netrctestEmpty";

}

class TestNetrcParser : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
       QFile netrc(testfileC);
       QVERIFY(netrc.open(QIODevice::WriteOnly));
       netrc.write("machine foo login bar password baz\n");
       netrc.write("machine broken login bar2 dontbelonghere password baz2 extratokens dontcare andanother\n");
       netrc.write("machine\nfunnysplit\tlogin bar3 password baz3\n");
       QFile netrcWithDefault(testfileWithDefaultC);
       QVERIFY(netrcWithDefault.open(QIODevice::WriteOnly));
       netrcWithDefault.write("machine foo login bar password baz\n");
       netrcWithDefault.write("default login user password pass\n");
       QFile netrcEmpty(testfileEmptyC);
       QVERIFY(netrcEmpty.open(QIODevice::WriteOnly));
    }

    void cleanupTestCase() {
       QVERIFY(QFile::remove(testfileC));
       QVERIFY(QFile::remove(testfileWithDefaultC));
       QVERIFY(QFile::remove(testfileEmptyC));
    }

    void testValidNetrc() {
       NetrcParser parser(testfileC);
       QVERIFY(parser.parse());
       QCOMPARE(parser.find("foo"), qMakePair(QString("bar"), QString("baz")));
       QCOMPARE(parser.find("broken"), qMakePair(QString("bar2"), QString("baz2")));
       QCOMPARE(parser.find("funnysplit"), qMakePair(QString("bar3"), QString("baz3")));
    }

    void testEmptyNetrc() {
       NetrcParser parser(testfileEmptyC);
       QVERIFY(!parser.parse());
       QCOMPARE(parser.find("foo"), qMakePair(QString(), QString()));
    }

    void testValidNetrcWithDefault() {
       NetrcParser parser(testfileWithDefaultC);
       QVERIFY(parser.parse());
       QCOMPARE(parser.find("foo"), qMakePair(QString("bar"), QString("baz")));
       QCOMPARE(parser.find("dontknow"), qMakePair(QString("user"), QString("pass")));
    }

    void testInvalidNetrc() {
       NetrcParser parser("/invalid");
       QVERIFY(!parser.parse());
    }
};

#endif
