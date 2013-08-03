/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#ifndef MIRALL_TESTFIRST_H
#define MIRALL_TESTFIRST_H

#include <QtTest>


class TestFirst : public QObject
{
    Q_OBJECT

private slots:
    void testTheFirstThing()
    {
        QVERIFY( true );
    }
};

#endif
