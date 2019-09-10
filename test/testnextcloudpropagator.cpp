/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#include <QtTest>
#include <QDebug>

#include "propagatedownload.h"
#include "owncloudpropagator_p.h"

using namespace OCC;
namespace OCC {
QString OWNCLOUDSYNC_EXPORT createDownloadTmpFileName(const QString &previous);
}

class TestNextcloudPropagator : public QObject
{
    Q_OBJECT

private slots:
    void testUpdateErrorFromSession()
    {
//        NextcloudPropagator propagator( NULL, QLatin1String("test1"), QLatin1String("test2"), new ProgressDatabase);
        QVERIFY( true );
    }

    void testTmpDownloadFileNameGeneration()
    {
        QString fn;
        // without dir
        for (int i = 1; i <= 1000; i++) {
            fn+="F";
            QString tmpFileName = createDownloadTmpFileName(fn);
            if (tmpFileName.contains('/')) {
                tmpFileName = tmpFileName.mid(tmpFileName.lastIndexOf('/')+1);
            }
            QVERIFY( tmpFileName.length() > 0);
            QVERIFY( tmpFileName.length() <= 254);
        }
        // with absolute dir
        fn = "/Users/guruz/ownCloud/rocks/GPL";
        for (int i = 1; i < 1000; i++) {
            fn+="F";
            QString tmpFileName = createDownloadTmpFileName(fn);
            if (tmpFileName.contains('/')) {
                tmpFileName = tmpFileName.mid(tmpFileName.lastIndexOf('/')+1);
            }
            QVERIFY( tmpFileName.length() > 0);
            QVERIFY( tmpFileName.length() <= 254);
        }
        // with relative dir
        fn = "rocks/GPL";
        for (int i = 1; i < 1000; i++) {
            fn+="F";
            QString tmpFileName = createDownloadTmpFileName(fn);
            if (tmpFileName.contains('/')) {
                tmpFileName = tmpFileName.mid(tmpFileName.lastIndexOf('/')+1);
            }
            QVERIFY( tmpFileName.length() > 0);
            QVERIFY( tmpFileName.length() <= 254);
        }
    }

    void testParseEtag()
    {
        typedef QPair<const char*, const char*> Test;
        QList<Test> tests;
        tests.append(Test("\"abcd\"", "abcd"));
        tests.append(Test("\"\"", ""));
        tests.append(Test("\"fii\"-gzip", "fii"));
        tests.append(Test("W/\"foo\"", "foo"));

        foreach (const auto& test, tests) {
            QCOMPARE(parseEtag(test.first), QByteArray(test.second));
        }
    }
};

QTEST_APPLESS_MAIN(TestNextcloudPropagator)
#include "testnextcloudpropagator.moc"
