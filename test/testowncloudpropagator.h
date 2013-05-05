/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#ifndef MIRALL_TESTOWNCLOUDPROPAGATOR_H
#define MIRALL_TESTOWNCLOUDPROPAGATOR_H

#include <QtTest>

#include "mirall/owncloudpropagator.h"

using namespace Mirall;

class TestOwncloudPropagator : public QObject
{
    Q_OBJECT

private slots:
    void testUpdateErrorFromSession()
    {
	OwncloudPropagator propagator( NULL, QLatin1String("test1"), QLatin1String("test2"));
        QVERIFY( true );
    }
};

#endif
