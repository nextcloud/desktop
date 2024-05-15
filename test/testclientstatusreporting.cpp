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
#include "clientstatusreportingcommon.h"
#include "clientstatusreportingdatabase.h"
#include "clientstatusreportingnetwork.h"
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
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval = 1000;
        OCC::ClientStatusReportingNetwork::repordSendIntervalMs = 2000;

        fakeQnam.reset(new FakeQNAM({}));
        account = OCC::Account::create().get();
        account->setCredentials(new FakeCredentials{fakeQnam.data()});
        account->setUrl(QUrl(("http://example.de")));

        accountState.reset(new OCC::AccountState(account->sharedFromThis()));

        const auto databaseId = QStringLiteral("%1@%2").arg(account->davUser(), account->url().toString());
        const auto databaseIdHash = QCryptographicHash::hash(databaseId.toUtf8(), QCryptographicHash::Md5);
        dbFilePath = QDir::tempPath() + QStringLiteral("/.tests_userdata_%1.db").arg(QString::fromLatin1(databaseIdHash.left(6).toHex()));
        QFile(dbFilePath).remove();
        OCC::ClientStatusReportingDatabase::dbPathForTesting = dbFilePath;

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
            // 2 conflicts
            account->reportClientStatus(OCC::ClientStatusReportingStatus::DownloadError_ConflictInvalidCharacters);
            account->reportClientStatus(OCC::ClientStatusReportingStatus::DownloadError_ConflictCaseClash);

            // 3 problems
            account->reportClientStatus(OCC::ClientStatusReportingStatus::UploadError_ServerError);
            account->reportClientStatus(OCC::ClientStatusReportingStatus::DownloadError_ServerError);
            account->reportClientStatus(OCC::ClientStatusReportingStatus::DownloadError_Virtual_File_Hydration_Failure);
            // 3 occurances of case ClientStatusReportingStatus::UploadError_No_Write_Permissions
            account->reportClientStatus(OCC::ClientStatusReportingStatus::DownloadError_Virtual_File_Hydration_Failure);
            account->reportClientStatus(OCC::ClientStatusReportingStatus::DownloadError_Virtual_File_Hydration_Failure);

            // 2 occurances of E2EeError_GeneralError
            account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
            account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);

            // 3 occurances of case ClientStatusReportingStatus::UploadError_Virus_Detected
            account->reportClientStatus(OCC::ClientStatusReportingStatus::UploadError_Virus_Detected);
            account->reportClientStatus(OCC::ClientStatusReportingStatus::UploadError_Virus_Detected);
            account->reportClientStatus(OCC::ClientStatusReportingStatus::UploadError_Virus_Detected);

            QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);

            QVERIFY(!bodyReceivedAndParsed.isEmpty());

            // we must have 3 virus_detected category statuses
            const auto virusDetectedErrorsReceived = bodyReceivedAndParsed.value("virus_detected").toMap();
            QVERIFY(!virusDetectedErrorsReceived.isEmpty());
            QCOMPARE(virusDetectedErrorsReceived.value("count"), 3);

            // we must have 2 e2ee errors
            const auto e2eeErrorsReceived = bodyReceivedAndParsed.value("e2ee_errors").toMap();
            QVERIFY(!e2eeErrorsReceived.isEmpty());
            QCOMPARE(e2eeErrorsReceived.value("count"), 2);

            // we must have 2 conflicts
            const auto conflictsReceived = bodyReceivedAndParsed.value("sync_conflicts").toMap();
            QVERIFY(!conflictsReceived.isEmpty());
            QCOMPARE(conflictsReceived.value("count"), 2);

            // we must have 3 problems
            const auto problemsReceived = bodyReceivedAndParsed.value("problems").toMap();
            QVERIFY(!problemsReceived.isEmpty());
            QCOMPARE(problemsReceived.size(), 3);
            const auto specificProblemMultipleOccurances = problemsReceived.value(OCC::clientStatusstatusStringFromNumber(OCC::ClientStatusReportingStatus::DownloadError_Virtual_File_Hydration_Failure)).toMap();
            // among those, 3 occurances of specific problem
            QCOMPARE(specificProblemMultipleOccurances.value("count"), 3);

            bodyReceivedAndParsed.clear();
        }
    }

    void testNothingReportedAndNothingSent()
    {
        QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);
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
