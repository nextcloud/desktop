/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#ifndef MIRALL_TESTDANIMOSTINKT_H
#define MIRALL_TESTDANIMOSTINKT_H

#include <QtTest>


class TestDanimoStinkt : public QObject
{
    Q_OBJECT

private slots:
    void testBadSmell()
    {
        QVERIFY( true );
    }
};

#endif
