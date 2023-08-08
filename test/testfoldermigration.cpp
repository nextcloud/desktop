/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>

#include "accountmanager.h"
#include "common/utility.h"
#include "configfile.h"
#include "folderman.h"
#include "testutils/testutils.h"

using namespace OCC;

namespace {
auto Settings_2_4()
{
    return QStringLiteral(R"([Accounts]
                          version=2
                          0\Folders\1\localPath=
                          0\Folders\1\journalPath=._sync_2215dfc5505b.db
                          0\url=https://demo.owncloud.com
                          0\Folders\1\targetPath=/
                          0\http_oauth=true
                          0\serverVersion=10.8.0.4
                          0\Folders\1\paused=false
                          0\http_user=admin
                          0\Folders\1\ignoreHiddenFiles=true
                          0\authType=http
                          0\user=admin)");
}

}

class TestFolderMigration : public QObject
{
    Q_OBJECT
private:
    auto writeSettings(const QTemporaryDir &tmp, const QString &content)
    {
        QFile settingsFile(ConfigFile::configFile());
        OC_ENFORCE(settingsFile.open(QFile::WriteOnly));
        settingsFile.write(content.toUtf8());
        settingsFile.close();

        auto settings = ConfigFile::settingsWithGroup(QStringLiteral("Accounts"));
        settings->setValue(QStringLiteral("0/Folders/1/localPath"), tmp.path());
        qDebug() << settings->childGroups() << settings->childKeys();


        return settings;
    }
private slots:
    void testFolderMigrationMissingJournalPath_data()
    {
        QTest::addColumn<QStringList>("journalPaths");
        QTest::addColumn<QString>("url");

        QTest::newRow("2.4") << QStringList{QStringLiteral("._sync_2215dfc5505b.db")} << QStringLiteral("https://demo.owncloud.com");
        QTest::newRow("2.4 url") << QStringList{QStringLiteral("._sync_2215dfc5505b.db")} << QStringLiteral("https://demo.owncloud.com/");
        QTest::newRow("2.6") << QStringList{QStringLiteral(".sync_2215dfc5505b.db")} << QStringLiteral("https://demo.owncloud.com");
        QTest::newRow("2.6 url") << QStringList{QStringLiteral(".sync_2215dfc5505b.db")} << QStringLiteral("https://demo.owncloud.com/");
        QTest::newRow("2.6 multi") << QStringList{QStringLiteral(".sync_2215dfc5505b.db"), QStringLiteral("._sync_2215dfc5505b.db")}
                                   << QStringLiteral("https://demo.owncloud.com");
        QTest::newRow("2.9") << QStringList{QStringLiteral(".sync_journal.db")} << QStringLiteral("https://demo.owncloud.com");
        QTest::newRow("2.9 url") << QStringList{QStringLiteral(".sync_journal.db")} << QStringLiteral("https://demo.owncloud.com/");
        QTest::newRow("2.9 multi") << QStringList{QStringLiteral(".sync_journal.db"), QStringLiteral(".sync_2215dfc5505b.db"),
            QStringLiteral("._sync_2215dfc5505b.db")}
                                   << "https://demo.owncloud.com";
    }

    void testFolderMigrationMissingJournalPath()
    {
        QFETCH(QStringList, journalPaths);
        QFETCH(QString, url);
        auto tmp = OCC::TestUtils::createTempDir();
        const auto settings = writeSettings(tmp, Settings_2_4());
        settings->setValue(QStringLiteral("0/url"), url);
        settings->remove(QStringLiteral("0/Folders/1/journalPath"));
        QVERIFY(!settings->value(QStringLiteral("0/Folders/1/journalPath")).isValid());

        for (const auto &journalPath : journalPaths) {
            QFile syncDb(tmp.filePath(journalPath));
            QVERIFY(syncDb.open(QFile::WriteOnly));
            syncDb.write("foo");
            syncDb.close();
        }

        AccountManager::instance()->restore();

        settings->beginGroup(QStringLiteral("0/Folders"));
        TestUtils::folderMan()->setupFoldersHelper(*settings.get(), AccountManager::instance()->accounts().first());
        settings->endGroup();

        QCOMPARE(journalPaths.first(), settings->value(QStringLiteral("0/Folders/1/journalPath")));
        delete TestUtils::folderMan();
    }
};

QTEST_MAIN(TestFolderMigration)
#include "testfoldermigration.moc"
