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

private Q_SLOTS:
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
        info.setVersionString(QStringLiteral("ownCloud 2.2.4 (build 6408)"));
        // esnure we do the update
        info.setVersion(QStringLiteral("100.2.4.6408"));
        auto *updater = new WindowsUpdater({});
        QSignalSpy downloadSpy(updater, &WindowsUpdater::downloadStateChanged);
        updater->versionInfoArrived(info);
        // we might have multiple state changes allow them to happen
        for (int i = 0; i <= OCUpdater::UpdateOnlyAvailableThroughSystem; ++i) {
            // wait up to a minute, we are actually downloading a file
            if (!downloadSpy.wait(60000)) {
                // we timed out
                break;
            }
            if (updater->downloadState() == result) {
                break;
            }
        }
        QCOMPARE(updater->downloadState(), result);
        updater->deleteLater();
    }
};
}

QTEST_MAIN(OCC::TestUpdater)
#include "testupdater.moc"
