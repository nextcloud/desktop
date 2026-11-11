/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "accountstate.h"
#include "clientstatusreportingcommon.h"
#include "clientstatusreportingdatabase.h"
#include "clientstatusreportingnetwork.h"
#include "configfile.h"
#include "folderman.h"
#include "syncenginetestutils.h"
#include "testhelper.h"
#include "tray/usermodel.h"

#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
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
    OCC::AccountStatePtr accountState;
    QTemporaryDir configDir;
    QString dbFilePath;
    QVariantMap bodyReceivedAndParsed;

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(configDir.isValid());
        OCC::ConfigFile::setConfDir(configDir.path());

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
            if (req.url().path() != u"/ocs/v2.php/apps/security_guard/diagnostics"_s) {
                return static_cast<QNetworkReply *>(nullptr);
            }
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

    void testFailedSyncStillReportsErrors_data()
    {
        QTest::addColumn<bool>("cachedReport");
        QTest::addColumn<bool>("queuedConflicts");
        QTest::newRow("startup") << false << false;
        QTest::newRow("startup with conflict events") << false << true;
        QTest::newRow("cached conflicts") << true << false;
        QTest::newRow("cached conflicts and new events") << true << true;
    }

    void testFailedSyncStillReportsErrors()
    {
        QFETCH(bool, cachedReport);
        QFETCH(bool, queuedConflicts);
        OCC::ClientStatusReportingDatabase database(account);
        QVERIFY(database.deleteClientStatusReportingRecords().isValid());
        const auto conflicts = QVariantMap{{u"count"_s, 5}, {u"oldest"_s, 10}};
        database.setLastSentReport(cachedReport ? QVariantMap{{u"sync_conflicts"_s, conflicts}} : QVariantMap{});
        database.setLastSentReportTimestamp(0);
        account->setServerErrorsResolvedAt(0);
        bodyReceivedAndParsed.clear();

        auto folderDir = QTemporaryDir{};
        QVERIFY(folderDir.isValid());
        const auto folderMan = OCC::FolderMan::instance();
        folderMan->setSyncEnabled(false);
        const auto folder = folderMan->addFolder(accountState.data(), folderDefinition(folderDir.path()));
        QVERIFY(folder);
        const auto removeFolder = qScopeGuard([folderMan, folder] {
            folderMan->removeFolder(folder);
        });
        auto user = OCC::User{accountState};
        auto &engine = folder->syncEngine();
        // Only the inventory callbacks run here; Folder's completion slot requires a started sync.
        QObject::disconnect(&engine, &OCC::SyncEngine::finished, folder, nullptr);
        engine.progressInfo()->_status = OCC::ProgressInfo::Reconcile;
        user.slotProgressInfo(folder->alias(), *engine.progressInfo());
        user.slotAddError(folder->alias(), u"Upload failed"_s, OCC::ErrorCategory::GenericError);
        engine.progressInfo()->_status = OCC::ProgressInfo::Done;
        Q_EMIT engine.finished(false);
        QVERIFY(!account->syncConflictReport());

        if (queuedConflicts) {
            account->reportClientStatus(OCC::ClientStatusReportingStatus::DownloadError_ConflictCaseClash);
        }
        QTest::qWait(OCC::ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval + OCC::ClientStatusReportingNetwork::repordSendIntervalMs);
        QVERIFY(bodyReceivedAndParsed.isEmpty());

        account->reportClientStatus(OCC::ClientStatusReportingStatus::UploadError_ServerError);
        account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
        account->reportClientStatus(OCC::ClientStatusReportingStatus::UploadError_Virus_Detected);
        QTRY_VERIFY(!bodyReceivedAndParsed.isEmpty());
        const auto uploadKey = QString::fromUtf8(OCC::clientStatusstatusStringFromNumber(OCC::ClientStatusReportingStatus::UploadError_ServerError));
        QCOMPARE(bodyReceivedAndParsed.value(u"problems"_s).toMap().value(uploadKey).toMap().value(u"count"_s).toInt(), 1);
        QCOMPARE(bodyReceivedAndParsed.value(u"e2ee_errors"_s).toMap().value(u"count"_s).toInt(), 1);
        QCOMPARE(bodyReceivedAndParsed.value(u"virus_detected"_s).toMap().value(u"count"_s).toInt(), 1);
        if (cachedReport) {
            QCOMPARE(bodyReceivedAndParsed.value(u"sync_conflicts"_s).toMap(), conflicts);
        } else {
            QCOMPARE(bodyReceivedAndParsed.value(u"sync_conflicts"_s).toMap().value(u"count"_s).toInt(), queuedConflicts ? 1 : 0);
        }
        QTRY_VERIFY(database.getClientStatusReportingRecords().isEmpty());

        // A successful full sync clears the reported transfer error and preserves other categories.
        const auto sentReport = bodyReceivedAndParsed;
        bodyReceivedAndParsed.clear();
        engine.progressInfo()->_status = OCC::ProgressInfo::Reconcile;
        user.slotProgressInfo(folder->alias(), *engine.progressInfo());
        engine.progressInfo()->_status = OCC::ProgressInfo::Done;
        Q_EMIT engine.finished(true);
        QVERIFY(account->syncConflictReport());
        QVERIFY(account->syncConflictReport()->isEmpty());
        database.setLastSentReportTimestamp(0);
        QTRY_VERIFY(!bodyReceivedAndParsed.isEmpty());
        QVERIFY(bodyReceivedAndParsed.value(u"problems"_s).toMap().isEmpty());
        QVERIFY(bodyReceivedAndParsed.value(u"sync_conflicts"_s).toMap().isEmpty());
        QCOMPARE(bodyReceivedAndParsed.value(u"e2ee_errors"_s), sentReport.value(u"e2ee_errors"_s));
        QCOMPARE(bodyReceivedAndParsed.value(u"virus_detected"_s), sentReport.value(u"virus_detected"_s));
        QTRY_COMPARE(database.lastSentReport(), bodyReceivedAndParsed);
    }

    void testConflictInventoryFromActivities_data()
    {
        using Status = OCC::SyncFileItem::Status;
        QTest::addColumn<QList<Status>>("statuses");
        QTest::addColumn<bool>("success");
        QTest::addColumn<int>("expectedCount");
        QTest::newRow("ordinary conflict") << QList<Status>{Status::Conflict} << false << 1;
        QTest::newRow("case clash") << QList<Status>{Status::FileNameClash} << false << 1;
        QTest::newRow("invalid filename") << QList<Status>{Status::FileNameInvalid} << false << 1;
        QTest::newRow("successful run with case clash") << QList<Status>{Status::FileNameClash} << true << 1;
        QTest::newRow("successful run with invalid filename") << QList<Status>{Status::FileNameInvalid} << true << 1;
        QTest::newRow("mixed errors") << QList<Status>{Status::NormalError, Status::FileNameClash, Status::FileNameInvalid, Status::Conflict} << false << 3;
        QTest::newRow("resolved") << QList<Status>{} << true << 0;
    }

    void testConflictInventoryFromActivities()
    {
        QFETCH(QList<OCC::SyncFileItem::Status>, statuses);
        QFETCH(bool, success);
        QFETCH(int, expectedCount);
        auto folderDir = QTemporaryDir{};
        QVERIFY(folderDir.isValid());
        const auto folderMan = OCC::FolderMan::instance();
        folderMan->setSyncEnabled(false);
        const auto folder = folderMan->addFolder(accountState.data(), folderDefinition(folderDir.path()));
        QVERIFY(folder);
        const auto removeFolder = qScopeGuard([folderMan, folder] {
            folderMan->removeFolder(folder);
        });
        auto user = OCC::User{accountState};
        auto &engine = folder->syncEngine();
        QObject::disconnect(&engine, &OCC::SyncEngine::finished, folder, nullptr);
        engine.progressInfo()->_status = OCC::ProgressInfo::Reconcile;
        user.slotProgressInfo(folder->alias(), *engine.progressInfo());
        QVERIFY(!account->syncConflictReport());

        auto timestamp = qint64{1};
        for (const auto status : statuses) {
            auto activity = OCC::Activity{};
            activity._type = OCC::Activity::SyncFileItemType;
            activity._folder = folder->alias();
            activity._file = QString::number(timestamp);
            activity._id = timestamp;
            activity._dateTime = QDateTime::fromMSecsSinceEpoch(timestamp++);
            activity._syncFileItemStatus = status;
            user.getActivityModel()->addErrorToActivityList(activity, OCC::ActivityListModel::ErrorType::SyncError);
        }

        engine.progressInfo()->_status = OCC::ProgressInfo::Done;
        Q_EMIT engine.finished(success);
        QVERIFY(account->syncConflictReport());
        if (expectedCount == 0) {
            QVERIFY(account->syncConflictReport()->isEmpty());
        } else {
            QCOMPARE(account->syncConflictReport()->value(u"count"_s).toInt(), expectedCount);
            QCOMPARE(account->syncConflictReport()->value(u"oldest"_s).toLongLong(), statuses.first() == OCC::SyncFileItem::NormalError ? 2 : 1);
        }
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
