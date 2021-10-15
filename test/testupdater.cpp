/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#include <QtTest>

#include "updater/updater.h"
#include "updater/ocupdater.h"

namespace OCC {

class TestUpdater : public QObject
{
    Q_OBJECT

private slots:
    void testVersionToInt()
    {
        auto lowVersion = Updater::Helper::versionToInt(1, 2, 80, 3000);
        QCOMPARE(Updater::Helper::stringVersionToInt("1.2.80.3000"), lowVersion);

        auto highVersion = Updater::Helper::versionToInt(999, 2, 80, 3000);
        auto currVersion = Updater::Helper::currentVersionToInt();
        QVERIFY(currVersion > 0);
        QVERIFY(currVersion > lowVersion);
        QVERIFY(currVersion < highVersion);
    }

    void testDownload_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<OCUpdater::DownloadState>("result");
        // a redirect to attic
        QTest::newRow("redirect") << "https://download.owncloud.com/desktop/stable/ownCloud-2.2.4.6408-setup.exe" << OCUpdater::DownloadComplete;
        QTest::newRow("broken url") << "https://&" << OCUpdater::DownloadFailed;
    }

    void testDownload()
    {
        QFETCH(QString, url);
        QFETCH(OCUpdater::DownloadState, result);
        UpdateInfo info;
        info.setDownloadUrl(url);
        info.setVersionString("ownCloud 2.2.4 (build 6408)");
        // esnure we do the update
        info.setVersion("100.2.4.6408");
        auto *updater = new NSISUpdater({});
        QSignalSpy downloadSpy(updater, &NSISUpdater::slotDownloadFinished);
        updater->versionInfoArrived(info);
        downloadSpy.wait();
        QCOMPARE(updater->downloadState(), result);
        updater->deleteLater();
    }
};
}

QTEST_MAIN(OCC::TestUpdater)
#include "testupdater.moc"
