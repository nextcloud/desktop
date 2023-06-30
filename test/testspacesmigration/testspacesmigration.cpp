/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#include <QtTest>

#include "testutils/syncenginetestutils.h"

#include "gui/spacemigration.h"


#include <QJsonArray>
#include <QJsonObject>

using namespace OCC;

class TestSpacesMigration : public QObject
{
    Q_OBJECT
private:
    const QTemporaryDir _tmp = TestUtils::createTempDir();

    auto addFolder(AccountStatePtr accountState, const QString &localPath, const QString &remotePath)
    {
        auto d = OCC::FolderDefinition::createNewFolderDefinition(accountState->account()->davUrl());
        Q_ASSERT(localPath.startsWith(QLatin1Char('/')));
        d.setLocalPath(_tmp.path() + localPath);
        d.setTargetPath(remotePath);
        return TestUtils::folderMan()->addFolder(accountState, d);
    }

private slots:
    void test()
    {
        FakeFolder fakeFolder({});
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            QString payloadName;
            if (op == QNetworkAccessManager::PostOperation && request.url().path().endsWith(QLatin1String("migration/spaces"))) {
                payloadName = QStringLiteral("migrationpayload.json");
            } else if (op == QNetworkAccessManager::GetOperation && request.url().path().endsWith(QLatin1String("graph/v1.0/me/drives"))) {
                payloadName = QStringLiteral("drivespayload.json");
            }
            if (payloadName.isEmpty()) {
                return nullptr;
            } else {
                QFile f(QStringLiteral(SOURCEDIR "/test/testspacesmigration/%1").arg(payloadName));
                Q_ASSERT(f.open(QIODevice::ReadOnly));
                return new FakePayloadReply(op, request, f.readAll(), this);
            }
        });
        auto cap = TestUtils::testCapabilities();
        cap[QStringLiteral("migration")] = QVariantMap{{QStringLiteral("space_migration"),
            QVariantMap{{QStringLiteral("enabled"), true}, {QStringLiteral("endpoint"), QStringLiteral("migration/spaces")}}}};
        fakeFolder.account()->setCapabilities(cap);


        const QUrl personalUrl(
            QStringLiteral("https://ocis.owncloud.test/dav/spaces/1284d238-aa92-42ce-bdc4-0b0000009157$534bb038-6f9d-4093-946f-133be61fa4e7"));
        QByteArray folder1Uuid;
        auto newAccountState = AccountState::fromNewAccount(fakeFolder.account());
        {
            auto folder1 = addFolder(newAccountState.get(), QStringLiteral("/root"), QStringLiteral("/"));
            folder1Uuid = folder1->id();

            auto folder2 = addFolder(newAccountState.get(), QStringLiteral("/Documents"), QStringLiteral("/Documents"));
            auto folder3 = addFolder(newAccountState.get(), QStringLiteral("/eos/a/Alice"), QStringLiteral("/eos/a/Alice"));
            auto folder4 = addFolder(newAccountState.get(), QStringLiteral("/Shares"), QStringLiteral("/Shares"));


            QVERIFY(fakeFolder.account()->capabilities().migration().space_migration.enabled);
            auto *migr = new SpaceMigration(newAccountState.get(), fakeFolder.account()->capabilities().migration().space_migration.endpoint, this);
            QSignalSpy spy(migr, &SpaceMigration::finished);
            migr->start();
            QVERIFY(spy.wait());
            migr->deleteLater();

            QCOMPARE(folder1->webDavUrl(), personalUrl);
            QCOMPARE(folder1->remotePath(), QStringLiteral("/"));

            QCOMPARE(folder2->webDavUrl(), personalUrl);
            QCOMPARE(folder2->remotePath(), QStringLiteral("/Documents"));

            QCOMPARE(folder3->webDavUrl(),
                QUrl(QStringLiteral("https://ocis.owncloud.test/dav/spaces/1284d238-aa92-42ce-bdc4-0b0000009157$31f599a1-94e1-4dfb-8bc9-841a57066d05")));
            QCOMPARE(folder3->remotePath(), QStringLiteral("/"));

            // The shares folder was not migrated
            // TODO: should shares be disabled by the migration?
            QCOMPARE(folder4->webDavUrl(), fakeFolder.account()->davUrl());
            QCOMPARE(folder4->remotePath(), QStringLiteral("/Shares"));
        }

        const int expectedSize = 4;
        QCOMPARE(FolderMan::instance()->folders().size(), expectedSize);

        // unload the folders
        FolderMan::instance()->unloadAndDeleteAllFolders();
        QVERIFY(FolderMan::instance()->folders().isEmpty());
        // reload the folders from the settings
        QCOMPARE(FolderMan::instance()->setupFolders(), expectedSize);

        // was the folder correctly persisted
        const auto folder = FolderMan::instance()->folder(folder1Uuid);
        QVERIFY(folder);
        QCOMPARE(folder->webDavUrl(), personalUrl);
        QCOMPARE(folder->remotePath(), QStringLiteral("/"));
    }
};

QTEST_MAIN(TestSpacesMigration)
#include "testspacesmigration.moc"
