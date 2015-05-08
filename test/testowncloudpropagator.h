/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#ifndef MIRALL_TESTOWNCLOUDPROPAGATOR_H
#define MIRALL_TESTOWNCLOUDPROPAGATOR_H

#include <QtTest>
#include <QDebug>

#include "propagatedownload.h"

using namespace OCC;
namespace OCC {
QString createDownloadTmpFileName(const QString &previous);
}

class TestOwncloudPropagator : public QObject
{
    Q_OBJECT

private slots:
    void testUpdateErrorFromSession()
    {
//        OwncloudPropagator propagator( NULL, QLatin1String("test1"), QLatin1String("test2"), new ProgressDatabase);
        QVERIFY( true );
    }

    void testTmpDownloadFileNameGeneration()
    {
        QString fn;
        for (int i = 1; i < 1000; i++) {
            fn+="F";
            QString tmpFileName = createDownloadTmpFileName(fn);
            QVERIFY( tmpFileName.length() > 0);
            QVERIFY( tmpFileName.length() <= 254);
        }
    }
};

#endif
