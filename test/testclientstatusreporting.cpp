/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "accountstate.h"
#include "clientstatusreportingcommon.h"
#include "clientstatusreportingdatabase.h"
#include "clientstatusreportingnetwork.h"
#include "syncenginetestutils.h"

#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

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

private Q_SLOTS:
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

    void testCurrentConflictsAndCleanup()
    {
        const auto oldest = QDateTime::currentMSecsSinceEpoch();
        const auto snapshot = QVariantMap{{u"count"_s, 5}, {u"oldest"_s, oldest}};
        account->setSyncConflictReport(snapshot);
        for (auto i = 0; i < 10; ++i) {
            account->reportClientStatus(OCC::ClientStatusReportingStatus::DownloadError_ConflictCaseClash);
        }
        account->reportClientStatus(OCC::ClientStatusReportingStatus::DownloadError_ServerError);
        QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);
        QCOMPARE(bodyReceivedAndParsed.value(u"sync_conflicts"_s).toMap(), snapshot);
        const auto problems = bodyReceivedAndParsed.value(u"problems"_s).toMap();
        QVERIFY(!problems.isEmpty());
        bodyReceivedAndParsed.clear();

        // A restart or reconciliation has no inventory yet, rather than zero conflicts.
        account->setSyncConflictReport(std::nullopt);
        account->setCapabilities({});
        account->setCapabilities({{u"security_guard"_s, QVariantMap{{u"diagnostics"_s, true}}}});
        QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);
        QVERIFY(bodyReceivedAndParsed.isEmpty());

        // Restoring the same inventory must not submit another report.
        account->setSyncConflictReport(snapshot);
        QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);
        QVERIFY(bodyReceivedAndParsed.isEmpty());

        // A malformed acknowledgement must not discard the report or suppress retries.
        fake200Response = "{}";
        account->setSyncConflictReport(QVariantMap{});
        account->reportClientStatus(OCC::ClientStatusReportingStatus::DownloadError_ConflictCaseClash);
        QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);
        QVERIFY(!bodyReceivedAndParsed.isEmpty());
        QVERIFY(bodyReceivedAndParsed.value(u"sync_conflicts"_s).toMap().isEmpty());
        QCOMPARE(bodyReceivedAndParsed.value(u"problems"_s).toMap(), problems);
        bodyReceivedAndParsed.clear();

        fake200Response = R"({"ocs":{"meta":{"status":"failure","statuscode":500}}})";
        QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);
        QVERIFY(!bodyReceivedAndParsed.isEmpty());
        QCOMPARE(bodyReceivedAndParsed.value(u"problems"_s).toMap(), problems);
        bodyReceivedAndParsed.clear();

        fake200Response = R"({"ocs":{"meta":{"status":"success","statuscode":200},"data":[]}})";
        QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);
        QVERIFY(!bodyReceivedAndParsed.isEmpty());
        QVERIFY(bodyReceivedAndParsed.value(u"sync_conflicts"_s).toMap().isEmpty());
        QCOMPARE(bodyReceivedAndParsed.value(u"problems"_s).toMap(), problems);
        bodyReceivedAndParsed.clear();
        QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);
        QVERIFY(bodyReceivedAndParsed.isEmpty());
        // Ordinary file conflicts need a report even without legacy conflict events.
        account->setSyncConflictReport(snapshot);
        QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);
        QCOMPARE(bodyReceivedAndParsed.value(u"sync_conflicts"_s).toMap(), snapshot);
        QCOMPARE(bodyReceivedAndParsed.value(u"problems"_s).toMap(), problems);
        bodyReceivedAndParsed.clear();
        account->setSyncConflictReport(std::nullopt);
    }

    void testServerErrorCleanupPreservesOtherCategories_data()
    {
        QTest::addColumn<bool>("keepOtherProblems");
        QTest::newRow("preserve other problems") << true;
        QTest::newRow("empty problems category") << false;
    }

    void testServerErrorCleanupPreservesOtherCategories()
    {
        QFETCH(bool, keepOtherProblems);
        OCC::ClientStatusReportingDatabase database(account);
        const auto issue = QVariantMap{{u"count"_s, 3}, {u"oldest"_s, 10}};
        const auto uploadKey = QString::fromUtf8(OCC::clientStatusstatusStringFromNumber(OCC::ClientStatusReportingStatus::UploadError_ServerError));
        const auto downloadKey = QString::fromUtf8(OCC::clientStatusstatusStringFromNumber(OCC::ClientStatusReportingStatus::DownloadError_ServerError));
        const auto hydrationKey =
            QString::fromUtf8(OCC::clientStatusstatusStringFromNumber(OCC::ClientStatusReportingStatus::DownloadError_Virtual_File_Hydration_Failure));
        const auto retainedProblems = keepOtherProblems ? QVariantMap{{hydrationKey, issue}, {u"Unknown.Error"_s, issue}} : QVariantMap{};
        auto problems = retainedProblems;
        problems.insert(uploadKey, issue);
        problems.insert(downloadKey, issue);
        const auto previousReport =
            QVariantMap{{u"sync_conflicts"_s, QVariantMap{}}, {u"problems"_s, problems}, {u"e2ee_errors"_s, issue}, {u"virus_detected"_s, issue}};
        database.setLastSentReport(previousReport);
        database.setLastSentReportTimestamp(0);
        account->setSyncConflictReport(QVariantMap{});
        account->setServerErrorsResolvedAt(0);
        bodyReceivedAndParsed.clear();
        QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);
        QVERIFY(bodyReceivedAndParsed.isEmpty());

        OCC::ClientStatusReportingRecord record;
        record._status = static_cast<int>(OCC::ClientStatusReportingStatus::DownloadError_ServerError);
        record._name = downloadKey.toUtf8();
        record._lastOccurence = 10;
        QVERIFY(database.setClientStatusReportingRecord(record).isValid());
        account->setServerErrorsResolvedAt(20);
        QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);
        auto expectedReport = previousReport;
        expectedReport[u"problems"_s] = retainedProblems;
        QCOMPARE(bodyReceivedAndParsed, expectedReport);
        QVERIFY(database.getClientStatusReportingRecords().isEmpty());
        bodyReceivedAndParsed.clear();

        // New failures after recovery must still be reported.
        record._lastOccurence = 30;
        QVERIFY(database.setClientStatusReportingRecord(record).isValid());
        QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);
        QCOMPARE(bodyReceivedAndParsed.value(u"problems"_s).toMap().value(downloadKey).toMap().value(u"count"_s).toInt(), 1);
        bodyReceivedAndParsed.clear();
        account->setServerErrorsResolvedAt(0);
        account->setSyncConflictReport(std::nullopt);
    }

    void testReportCacheIsAccountLocalAndPersistent()
    {
        const auto otherPath = dbFilePath + u".other"_s;
        QFile::remove(otherPath);
        OCC::ClientStatusReportingDatabase::dbPathForTesting = otherPath;
        const auto report = QVariantMap{{u"sync_conflicts"_s, QVariantMap{{u"count"_s, 2}}}};
        {
            OCC::ClientStatusReportingDatabase otherDatabase(account);
            QVERIFY(otherDatabase.isInitialized());
            QVERIFY(otherDatabase.lastSentReport().isEmpty());
            otherDatabase.setLastSentReport(report);
            OCC::ClientStatusReportingDatabase::dbPathForTesting = dbFilePath;
            OCC::ClientStatusReportingDatabase originalDatabase(account);
            QVERIFY(originalDatabase.lastSentReport() != report);
            QCOMPARE(otherDatabase.lastSentReport(), report);
        }
        OCC::ClientStatusReportingDatabase::dbPathForTesting = otherPath;
        {
            OCC::ClientStatusReportingDatabase reopenedDatabase(account);
            QCOMPARE(reopenedDatabase.lastSentReport(), report);
        }
        OCC::ClientStatusReportingDatabase::dbPathForTesting = dbFilePath;
        QFile::remove(otherPath);
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
