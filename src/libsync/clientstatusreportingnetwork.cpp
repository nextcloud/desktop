/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "clientstatusreportingnetwork.h"

#include "account.h"
#include "clientstatusreportingdatabase.h"
#include "clientstatusreportingrecord.h"
#include <networkjobs.h>

namespace
{
constexpr auto statusReportCategoryE2eErrors = "e2ee_errors";
constexpr auto statusReportCategoryProblems = "problems";
constexpr auto statusReportCategorySyncConflicts = "sync_conflicts";
constexpr auto statusReportCategoryVirus = "virus_detected";
}

namespace OCC
{
Q_LOGGING_CATEGORY(lcClientStatusReportingNetwork, "nextcloud.sync.clientstatusreportingnetwork", QtInfoMsg)

ClientStatusReportingNetwork::ClientStatusReportingNetwork(Account *account, const QSharedPointer<ClientStatusReportingDatabase> database, QObject *parent)
    : QObject(parent)
    , _account(account)
    , _database(database)
{
    init();
}

ClientStatusReportingNetwork::~ClientStatusReportingNetwork()
{
}

void ClientStatusReportingNetwork::init()
{
    Q_ASSERT(!_isInitialized);
    if (_isInitialized) {
        return;
    }

    _clientStatusReportingSendTimer.setInterval(clientStatusReportingTrySendTimerInterval);
    connect(&_clientStatusReportingSendTimer, &QTimer::timeout, this, &ClientStatusReportingNetwork::sendReportToServer);
    _clientStatusReportingSendTimer.start();

    _isInitialized = true;
}

bool ClientStatusReportingNetwork::isInitialized() const
{
    return _isInitialized;
}

void ClientStatusReportingNetwork::sendReportToServer()
{
    if (!_isInitialized) {
        qCWarning(lcClientStatusReportingNetwork) << "Could not send client status report to server. Status reporting is not initialized";
        return;
    }

    const auto lastSentReportTime = _database->getLastSentReportTimestamp();
    if (QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() - lastSentReportTime < repordSendIntervalMs) {
        qCDebug(lcClientStatusReportingNetwork) << "Skipped sending client status report because interval is not met";
        return;
    }

    const auto report = prepareReport();
    if (report.isEmpty()) {
        qCDebug(lcClientStatusReportingNetwork) << "Skipped sending client status report because no records are queued";
        return;
    }

    if (!_account) {
        return;
    }

    const auto clientStatusReportingJob = new JsonApiJob(_account->sharedFromThis(), QStringLiteral("ocs/v2.php/apps/security_guard/diagnostics"));
    clientStatusReportingJob->setBody(QJsonDocument::fromVariant(report));
    clientStatusReportingJob->setVerb(SimpleApiJob::Verb::Put);
    connect(clientStatusReportingJob, &JsonApiJob::jsonReceived, [this](const QJsonDocument &json, int statusCode) {
        const auto isSuccess = statusCode == HttpErrorCodeNone || statusCode == HttpErrorCodeSuccess || statusCode == HttpErrorCodeSuccessCreated
            || statusCode == HttpErrorCodeSuccessNoContent;
        if (isSuccess) {
            const auto metaFromJson = json.object().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("meta")).toObject();
            const auto codeFromJson = metaFromJson.value(QStringLiteral("statuscode")).toInt();
            if (codeFromJson == HttpErrorCodeNone || codeFromJson == HttpErrorCodeSuccess || codeFromJson == HttpErrorCodeSuccessCreated
                || codeFromJson == HttpErrorCodeSuccessNoContent) {
                reportToServerSentSuccessfully();
                return;
            }
            qCWarning(lcClientStatusReportingNetwork) << "Received error when sending client status report statusCode:" << statusCode << "codeFromJson:" << codeFromJson;
        }
    });
    clientStatusReportingJob->start();
}

void ClientStatusReportingNetwork::reportToServerSentSuccessfully()
{
    qCInfo(lcClientStatusReportingNetwork) << "Succesfully sent client status report";
    if (!_database->deleteClientStatusReportingRecords()) {
        qCWarning(lcClientStatusReportingNetwork) << "Could not delete records after sending the report";
    }
    _database->setLastSentReportTimestamp(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
}

QVariantMap ClientStatusReportingNetwork::prepareReport() const
{
    const auto records = _database->getClientStatusReportingRecords();
    if (records.isEmpty()) {
        return {};
    }

    QVariantMap report;
    report[statusReportCategorySyncConflicts] = QVariantMap{};
    report[statusReportCategoryProblems] = QVariantMap{};
    report[statusReportCategoryVirus] = QVariantMap{};
    report[statusReportCategoryE2eErrors] = QVariantMap{};

    QVariantMap e2eeErrors;
    QVariantMap problems;
    QVariantMap syncConflicts;
    QVariantMap virusDetectedErrors;

    for (const auto &record : records) {
        const auto categoryKey = classifyStatus(static_cast<ClientStatusReportingStatus>(record._status));

        if (categoryKey.isEmpty()) {
            qCWarning(lcClientStatusReportingNetwork) << "Could not classify status:";
            continue;
        }

        if (categoryKey == statusReportCategoryE2eErrors) {
            const auto initialCount = e2eeErrors[QStringLiteral("count")].toInt();
            e2eeErrors[QStringLiteral("count")] = initialCount + record._numOccurences;
            e2eeErrors[QStringLiteral("oldest")] = record._lastOccurence;
            report[categoryKey] = e2eeErrors;
        } else if (categoryKey == statusReportCategoryProblems) {
            problems[record._name] = QVariantMap{{QStringLiteral("count"), record._numOccurences}, {QStringLiteral("oldest"), record._lastOccurence}};
            report[categoryKey] = problems;
        } else if (categoryKey == statusReportCategorySyncConflicts) {
            const auto initialCount = syncConflicts[QStringLiteral("count")].toInt();
            syncConflicts[QStringLiteral("count")] = initialCount + record._numOccurences;
            syncConflicts[QStringLiteral("oldest")] = record._lastOccurence;
            report[categoryKey] = syncConflicts;
        } else if (categoryKey == statusReportCategoryVirus) {
            const auto initialCount = virusDetectedErrors[QStringLiteral("count")].toInt();
            virusDetectedErrors[QStringLiteral("count")] = initialCount + record._numOccurences;
            virusDetectedErrors[QStringLiteral("oldest")] = record._lastOccurence;
            report[categoryKey] = virusDetectedErrors;
        }
    }
    return report;
}

QByteArray ClientStatusReportingNetwork::classifyStatus(const ClientStatusReportingStatus status)
{
    Q_ASSERT(static_cast<int>(status) >= 0 && static_cast<int>(status) < static_cast<int>(ClientStatusReportingStatus::Count));
    if (static_cast<int>(status) < 0 || static_cast<int>(status) >= static_cast<int>(ClientStatusReportingStatus::Count)) {
        qCWarning(lcClientStatusReportingNetwork) << "Invalid status:" << static_cast<int>(status);
        return {};
    }

    switch (status) {
    case ClientStatusReportingStatus::DownloadError_ConflictCaseClash:
    case ClientStatusReportingStatus::DownloadError_ConflictInvalidCharacters:
        return statusReportCategorySyncConflicts;
    case ClientStatusReportingStatus::DownloadError_ServerError:
    case ClientStatusReportingStatus::DownloadError_Virtual_File_Hydration_Failure:
    case ClientStatusReportingStatus::UploadError_ServerError:
        return statusReportCategoryProblems;
    case ClientStatusReportingStatus::UploadError_Virus_Detected:
        return statusReportCategoryVirus;
    case ClientStatusReportingStatus::E2EeError_GeneralError:
        return statusReportCategoryE2eErrors;
    case ClientStatusReportingStatus::Count:
        return {};
    };
    return {};
}

int ClientStatusReportingNetwork::clientStatusReportingTrySendTimerInterval = 1000 * 60 * 2; // check if the time has come, every 2 minutes
quint64 ClientStatusReportingNetwork::repordSendIntervalMs = 24 * 60 * 60 * 1000; // once every 24 hours
}
