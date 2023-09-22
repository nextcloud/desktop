/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#include <QtTest>
#include <QDebug>

#include "account.h"
#include "propagatedownload.h"
#include "owncloudpropagator_p.h"
#include "syncoptions.h"

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
        //OwncloudPropagator propagator(nullptr, QLatin1String("test1"), QLatin1String("test2"), new ProgressDatabase);
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
        using Test = QPair<const char*, const char*>;
        QList<Test> tests;
        tests.append(Test("\"abcd\"", "abcd"));
        tests.append(Test("\"\"", ""));
        tests.append(Test("\"fii\"-gzip", "fii"));
        tests.append(Test("W/\"foo\"", "foo"));

        foreach (const auto& test, tests) {
            QCOMPARE(parseEtag(test.first), QByteArray(test.second));
        }
    }

    void testRespectsLowestPossibleChunkSize()
    {
        QSet<QString> __blacklist;
        OwncloudPropagator propagator(Account::create(), "", "", nullptr, __blacklist);
        auto opts = propagator.syncOptions();

        opts.setMinChunkSize(0);
        opts.setMaxChunkSize(0);
        opts.setInitialChunkSize(0);

        propagator.setSyncOptions(opts);

        opts = propagator.syncOptions();
        QCOMPARE( opts.minChunkSize(), SyncOptions::chunkV2MinChunkSize );
        QCOMPARE( opts.initialChunkSize(), SyncOptions::chunkV2MinChunkSize );
        QCOMPARE( opts.maxChunkSize(), SyncOptions::chunkV2MinChunkSize );
    }

    void testLimitsMaxChunkSizeByAccount()
    {
        QSet<QString> __blacklist;
        OwncloudPropagator propagator(Account::create(), "", "", nullptr, __blacklist);
        auto opts = propagator.syncOptions();
        
        SyncOptions defaultOpts;
        QCOMPARE( opts.minChunkSize(), defaultOpts.minChunkSize() );
        QVERIFY( opts.minChunkSize() < defaultOpts.maxChunkSize() );
        QVERIFY( opts.minChunkSize() < defaultOpts.initialChunkSize() );
        QCOMPARE( opts.initialChunkSize(), defaultOpts.initialChunkSize() );
        QCOMPARE( opts.maxChunkSize(), defaultOpts.maxChunkSize() );

        propagator.account()->setMaxRequestSizeIfLower(defaultOpts.minChunkSize());
        propagator.setSyncOptions(opts);
        
        opts = propagator.syncOptions();
        QCOMPARE( opts.minChunkSize(), defaultOpts.minChunkSize() );
        QCOMPARE( opts.initialChunkSize(), defaultOpts.minChunkSize() );
        QCOMPARE( opts.maxChunkSize(), defaultOpts.minChunkSize() );
    }
};

QTEST_APPLESS_MAIN(TestNextcloudPropagator)
#include "testnextcloudpropagator.moc"
