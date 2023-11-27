/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
#include "account.h"
#include "accountstate.h"
#include "clientstatusreporting.h"
#include "syncenginetestutils.h"

#include <QSignalSpy>
#include <QTest>

namespace {
static QByteArray fake200Response = R"({"ocs":{"meta":{"status":"success","statuscode":200},"data":[]}})";
}

class TestClientStatusReporting : public QObject
{
    Q_OBJECT

public:
    TestClientStatusReporting() = default;

    QScopedPointer<FakeQNAM> fakeQnam;
    OCC::Account *account;
    QScopedPointer<OCC::AccountState> accountState;
    QString dbFilePath;
    QVariantMap bodyReceivedAndParsed;

private slots:
    void initTestCase()
    {
        OCC::ClientStatusReporting::clientStatusReportingTrySendTimerInterval = 1000;
        OCC::ClientStatusReporting::repordSendIntervalMs = 2000;

        fakeQnam.reset(new FakeQNAM({}));
        account = OCC::Account::create().get();
        account->setCredentials(new FakeCredentials{fakeQnam.data()});
        account->setUrl(QUrl(("http://example.de")));

        accountState.reset(new OCC::AccountState(account->sharedFromThis()));

        const auto databaseId = QStringLiteral("%1@%2").arg(account->davUser(), account->url().toString());
        const auto databaseIdHash = QCryptographicHash::hash(databaseId.toUtf8(), QCryptographicHash::Md5);
        dbFilePath = QDir::tempPath() + QStringLiteral("/.tests_userdata_%1.db").arg(QString::fromLatin1(databaseIdHash.left(6).toHex()));
        QFile(dbFilePath).remove();
        OCC::ClientStatusReporting::dbPathForTesting = dbFilePath;

        QVariantMap capabilities;
        capabilities[QStringLiteral("security_guard")] = QVariantMap{
            { QStringLiteral("diagnostics"), true }
        };
        account->setCapabilities(capabilities);

        fakeQnam->setOverride([this](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
            QNetworkReply *reply = nullptr;
            const auto reqBody = device->readAll();
            bodyReceivedAndParsed = QJsonDocument::fromJson(reqBody).toVariant().toMap();
            reply = new FakePayloadReply(op, req, fake200Response, fakeQnam.data());
            return reply;
        });
    }

    void testReportAndSendStatuses()
    {
        for (int i = 0; i < 2; ++i) {
            // 5 conflicts
            account->reportClientStatus(OCC::ClientStatusReporting::Status::UploadError_Conflict);
            account->reportClientStatus(OCC::ClientStatusReporting::Status::UploadError_ConflictInvalidCharacters);
            account->reportClientStatus(OCC::ClientStatusReporting::Status::DownloadError_Conflict);
            account->reportClientStatus(OCC::ClientStatusReporting::Status::DownloadError_ConflictInvalidCharacters);
            account->reportClientStatus(OCC::ClientStatusReporting::Status::DownloadError_ConflictCaseClash);

            // 4 problems
            account->reportClientStatus(OCC::ClientStatusReporting::Status::UploadError_ServerError);
            account->reportClientStatus(OCC::ClientStatusReporting::Status::DownloadError_ServerError);
            account->reportClientStatus(OCC::ClientStatusReporting::Status::DownloadError_Virtual_File_Hydration_Failure);
            // 3 occurances of UploadError_No_Write_Permissions
            account->reportClientStatus(OCC::ClientStatusReporting::Status::UploadError_No_Write_Permissions);
            account->reportClientStatus(OCC::ClientStatusReporting::Status::UploadError_No_Write_Permissions);
            account->reportClientStatus(OCC::ClientStatusReporting::Status::UploadError_No_Write_Permissions);
            QTest::qWait(OCC::ClientStatusReporting::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReporting::repordSendIntervalMs);

            QVERIFY(!bodyReceivedAndParsed.isEmpty());

            // we must have "virus_detected" and "e2e_errors" keys present (as required by server)
            QVERIFY(bodyReceivedAndParsed.contains("virus_detected"));
            QVERIFY(bodyReceivedAndParsed.contains("e2e_errors"));

            // we must have 5 conflicts
            const auto conflictsReceived = bodyReceivedAndParsed.value("sync_conflicts").toMap();
            QVERIFY(!conflictsReceived.isEmpty());
            QCOMPARE(conflictsReceived.value("count"), 5);

            // we must have 4 problems
            const auto problemsReceived = bodyReceivedAndParsed.value("problems").toMap();
            QVERIFY(!problemsReceived.isEmpty());
            QCOMPARE(problemsReceived.size(), 4);
            const auto problemsNoWritePermissions = problemsReceived.value(OCC::ClientStatusReporting::statusStringFromNumber(OCC::ClientStatusReporting::Status::UploadError_No_Write_Permissions)).toMap();
            // among those, 3 occurances of UploadError_No_Write_Permissions
            QCOMPARE(problemsNoWritePermissions.value("count"), 3);

            bodyReceivedAndParsed.clear();
        }
    }

    void testNothingReportedAndNothingSent()
    {
        QTest::qWait(OCC::ClientStatusReporting::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReporting::repordSendIntervalMs);
        QVERIFY(bodyReceivedAndParsed.isEmpty());
    }

    void cleanupTestCase()
    {
        accountState.reset(nullptr);
        delete account;
        QFile(dbFilePath).remove();
    }
};

QTEST_MAIN(TestClientStatusReporting)
#include "testclientstatusreporting.moc"
