/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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
#include "clientstatusreporting.h"
#include "creds/abstractcredentials.h"
#include "account.h"
#include "common/clientstatusreportingrecord.h"
#include "common/syncjournaldb.h"
#include <configfile.h>
#include <networkjobs.h>

namespace
{
constexpr auto lastSentReportTimestamp = "lastClientStatusReportSentTime";
constexpr auto repordSendIntervalMs = 24 * 60 * 60 * 1000;
constexpr int clientStatusReportingSendTimerInterval = 1000 * 60 * 2;
}

namespace OCC
{
Q_LOGGING_CATEGORY(lcClientStatusReporting, "nextcloud.sync.clientstatusreporting", QtInfoMsg)

ClientStatusReporting::ClientStatusReporting(Account *account, QObject *parent)
    : _account(account)
    , QObject(parent)
{
    init();
}

void ClientStatusReporting::init()
{
    if (_isInitialized) {
        qCDebug(lcClientStatusReporting) << "Double call to init";
        return;
    }

    for (int i = 0; i < ClientStatusReporting::Count; ++i) {
        const auto statusString = statusStringFromNumber(static_cast<Status>(i));
        _statusNamesAndHashes[i] = {statusString, SyncJournalDb::getPHash(statusString)};
    }

    const auto databaseId = QStringLiteral("%1@%2").arg(_account->davUser(), _account->url().toString());
    const auto databaseIdHash = QCryptographicHash::hash(databaseId.toUtf8(), QCryptographicHash::Md5);

    const QString journalPath = ConfigFile().configPath() + QStringLiteral(".userdata_%1.db").arg(QString::fromLatin1(databaseIdHash.left(6).toHex()));

    _database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    _database.setDatabaseName(journalPath);

    if (!_database.open()) {
        qCDebug(lcClientStatusReporting) << "Could not setup client reporting, database connection error.";
        return;
    }

    QSqlQuery query;
    const auto prepareResult = query.prepare(
        "CREATE TABLE IF NOT EXISTS clientstatusreporting("
        "nHash INTEGER(8) PRIMARY KEY,"
        "status INTEGER(8))"
        "name VARCHAR(4096),"
        "count INTEGER,"
        "lastOccurrence INTEGER(8))");
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReporting) << "Could not setup client clientstatusreporting table:" << query.lastError().text();
        return;
    }

    if (!query.prepare("CREATE TABLE IF NOT EXISTS keyvalue(key VARCHAR(4096), value VARCHAR(4096), PRIMARY KEY(key))") || !query.exec()) {
        qCDebug(lcClientStatusReporting) << "Could not setup client keyvalue table:" << query.lastError().text();
        return;
    }

    _clientStatusReportingSendTimer.setInterval(clientStatusReportingSendTimerInterval);
    connect(&_clientStatusReportingSendTimer, &QTimer::timeout, this, &ClientStatusReporting::sendReportToServer);
    _clientStatusReportingSendTimer.start();

    _isInitialized = true;

    reportClientStatus(Status::DownloadError_ConflictCaseClash);
    reportClientStatus(Status::DownloadError_ConflictInvalidCharacters);
    reportClientStatus(Status::UploadError_ServerError);
    reportClientStatus(Status::UploadError_ServerError);
    setLastSentReportTimestamp(QDateTime::currentDateTime().toMSecsSinceEpoch());

    auto records = getClientStatusReportingRecords();

    auto resDelete = deleteClientStatusReportingRecords();

    records = getClientStatusReportingRecords();

    auto res = getLastSentReportTimestamp();
}

QVector<ClientStatusReportingRecord> ClientStatusReporting::getClientStatusReportingRecords() const
{
    QVector<ClientStatusReportingRecord> records;

    QMutexLocker locker(&_mutex);

    QSqlQuery query;
    const auto prepareResult = query.prepare("SELECT * FROM clientstatusreporting");

    if (!prepareResult || !query.exec()) {
        const auto errorMessage = query.lastError().text();
        qCDebug(lcClientStatusReporting) << "Could not get records from clientstatusreporting:" << errorMessage;
        return records;
    }

    while (query.next()) {
        ClientStatusReportingRecord record;
        record._nameHash = query.value(query.record().indexOf("nHash")).toLongLong();
        record._status = query.value(query.record().indexOf("status")).toLongLong();
        record._name = query.value(query.record().indexOf("name")).toByteArray();
        record._numOccurences = query.value(query.record().indexOf("count")).toLongLong();
        record._lastOccurence = query.value(query.record().indexOf("lastOccurrence")).toLongLong();
        records.push_back(record);
    }
    return records;
}

bool ClientStatusReporting::deleteClientStatusReportingRecords()
{
    QSqlQuery query;
    const auto prepareResult = query.prepare("DELETE FROM clientstatusreporting");

    if (!prepareResult || !query.exec()) {
        const auto errorMessage = query.lastError().text();
        qCDebug(lcClientStatusReporting) << "Could not get records from clientstatusreporting:" << errorMessage;
        return false;
    }
    return true;
}

Result<void, QString> ClientStatusReporting::setClientStatusReportingRecord(const ClientStatusReportingRecord &record)
{
    Q_ASSERT(record.isValid());
    if (!record.isValid()) {
        qCWarning(lcClientStatusReporting) << "Failed to set ClientStatusReportingRecord";
        return {QStringLiteral("Invalid parameter")};
    }

    const auto recordCopy = record;

    QMutexLocker locker(&_mutex);

    QSqlQuery query;

    const auto prepareResult = query.prepare(
        "INSERT OR REPLACE INTO clientstatusreporting (nHash, name, count, lastOccurrence) VALUES(:nHash, :name, :status, :count, :lastOccurrence) ON CONFLICT(nHash) "
        "DO UPDATE SET count = count + 1, lastOccurrence = :lastOccurrence;");
    query.bindValue(":nHash", recordCopy._nameHash);
    query.bindValue(":name", recordCopy._name);
    query.bindValue(":status", recordCopy._status);
    query.bindValue(":count", 1);
    query.bindValue(":lastOccurrence", recordCopy._lastOccurence);

    if (!prepareResult || !query.exec()) {
        const auto errorMessage = query.lastError().text();
        qCDebug(lcClientStatusReporting) << "Could not report client status:" << errorMessage;
        return errorMessage;
    }

    return {};
}

void ClientStatusReporting::reportClientStatus(const Status status)
{
    if (!_isInitialized) {
        qCWarning(lcClientStatusReporting) << "Could not report status. Status reporting is not initialized";
        return;
    }
    Q_ASSERT(status >= 0 && status < Count);
    if (status < 0 || status >= Status::Count) {
        qCWarning(lcClientStatusReporting) << "Trying to report invalid status:" << status;
        return;
    }

    ClientStatusReportingRecord record;
    record._name = _statusNamesAndHashes[status].first;
    record._status = status;
    record._nameHash = _statusNamesAndHashes[status].second;
    record._lastOccurence = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    const auto result = setClientStatusReportingRecord(record);
    if (!result.isValid()) {
        qCWarning(lcClientStatusReporting) << "Could not report client status:" << result.error();
    }
}

void ClientStatusReporting::sendReportToServer()
{
    if (!_isInitialized) {
        qCWarning(lcClientStatusReporting) << "Could not send report to server. Status reporting is not initialized";
        return;
    }

    const auto lastSentReportTime = setLastSentReportTimestamp(0);
    if (QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() - lastSentReportTime < repordSendIntervalMs) {
        return;
    }

    const auto records = getClientStatusReportingRecords();
    if (!records.isEmpty()) {
        // send to server ->

        QVariantMap report;

        QVariantMap syncConflicts;
        QVariantMap problems;

        for (const auto &record : records) {
            const auto categoryKey = classifyStatus(static_cast<Status>(record._status));

            if (categoryKey.isEmpty()) {
                qCDebug(lcClientStatusReporting) << "Could not classify status:";
                continue;
            }

            if (categoryKey == QStringLiteral("sync_conflicts")) {
                const auto initialCount = syncConflicts[QStringLiteral("count")].toInt();
                syncConflicts[QStringLiteral("count")] = initialCount + record._numOccurences;
                syncConflicts[QStringLiteral("oldest")] = record._lastOccurence;
                report[categoryKey] = syncConflicts;
            } else if (categoryKey == QStringLiteral("problems")) {
                problems[record._name] = QVariantMap {
                    {QStringLiteral("count"), record._numOccurences},
                    {QStringLiteral("oldest"), record._lastOccurence}
                };
                report[categoryKey] = problems;
            }
        }

        if (report.isEmpty()) {
            qCDebug(lcClientStatusReporting) << "Report is empty.";
            return;
        }

        const auto clientStatusReportingJob = new JsonApiJob(_account->sharedFromThis(), QStringLiteral("ocs/v2.php/apps/security_guard/diagnostics"));
        clientStatusReportingJob->setBody(QJsonDocument::fromVariant(report));
        clientStatusReportingJob->setVerb(SimpleApiJob::Verb::Put);
        connect(clientStatusReportingJob, &JsonApiJob::jsonReceived, [this](const QJsonDocument &json) {
            const QJsonObject data = json.object().value("ocs").toObject().value("data").toObject();
            slotSendReportToserverFinished();
        });
        clientStatusReportingJob->start();
    }
}

void ClientStatusReporting::slotSendReportToserverFinished()
{
    if (!deleteClientStatusReportingRecords()) {
        qCWarning(lcClientStatusReporting) << "Error deleting client status report.";
    }
    setLastSentReportTimestamp(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
}

qulonglong ClientStatusReporting::getLastSentReportTimestamp() const
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare("SELECT value FROM keyvalue WHERE key = (:key)");
    query.bindValue(":key", lastSentReportTimestamp);
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReporting) << "Could not get last sent report timestamp from keyvalue table. No such record:" << lastSentReportTimestamp;
        return 0;
    }
    if (!query.next()) {
        qCDebug(lcClientStatusReporting) << "Could not get last sent report timestamp from keyvalue table:" << query.lastError().text();
        return 0;
    }

    int valueIndex = query.record().indexOf("value");
    return query.value(valueIndex).toULongLong();
}

bool ClientStatusReporting::setLastSentReportTimestamp(const qulonglong timestamp)
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare("INSERT OR REPLACE INTO keyvalue (key, value) VALUES(:key, :value);");
    query.bindValue(":key", lastSentReportTimestamp);
    query.bindValue(":value", timestamp);
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReporting) << "Could not set last sent report timestamp from keyvalue table. No such record:" << lastSentReportTimestamp;
        return false;
    }

    return true;
}

QByteArray ClientStatusReporting::statusStringFromNumber(const Status status)
{
    Q_ASSERT(status >= 0 && status < Count);
    if (status < 0 || status >= Status::Count) {
        qCWarning(lcClientStatusReporting) << "Invalid status:" << status;
        return {};
    }

    switch (status) {
    case DownloadError_ConflictInvalidCharacters:
        return QByteArrayLiteral("DownloadError.CONFLICT_INVALID_CHARACTERS");
    case DownloadError_ConflictCaseClash:
        return QByteArrayLiteral("DownloadError.CONFLICT_CASECLASH");
    case UploadError_ServerError:
        return QByteArrayLiteral("UploadError.SERVER_ERROR");
    case Count:
        return {};
    };
    return {};
}

QString ClientStatusReporting::classifyStatus(const Status status)
{
    Q_ASSERT(status >= 0 && status < Count);
    if (status < 0 || status >= Status::Count) {
        qCWarning(lcClientStatusReporting) << "Invalid status:" << status;
        return {};
    }

    switch (status) {
    case DownloadError_ConflictInvalidCharacters:
        return QStringLiteral("sync_conflicts");
    case DownloadError_ConflictCaseClash:
        return QStringLiteral("sync_conflicts");
    case UploadError_ServerError:
        return QByteArrayLiteral("problems");
    case Count:
        return {};
    };
    return {};
}
}
