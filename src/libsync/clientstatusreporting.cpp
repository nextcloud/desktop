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

#include "account.h"
#include "clientstatusreportingrecord.h"
#include <configfile.h>
#include "common/c_jhash.h"
#include <networkjobs.h>

namespace
{
constexpr auto lastSentReportTimestamp = "lastClientStatusReportSentTime";
constexpr auto statusNamesHash = "statusNamesHash";
}

namespace OCC
{
Q_LOGGING_CATEGORY(lcClientStatusReporting, "nextcloud.sync.clientstatusreporting", QtInfoMsg)

ClientStatusReporting::ClientStatusReporting(Account *account, QObject *parent)
    : QObject(parent)
    , _account(account)
{
    init();
}

ClientStatusReporting::~ClientStatusReporting()
{
    if (_database.isOpen()) {
        _database.close();
    }
}

void ClientStatusReporting::init()
{
    Q_ASSERT(!_isInitialized);
    if (_isInitialized) {
        qCDebug(lcClientStatusReporting) << "Double call to init";
        return;
    }

    for (int i = 0; i < ClientStatusReporting::Status::Count; ++i) {
        const auto statusString = statusStringFromNumber(static_cast<Status>(i));
        _statusNamesAndHashes[i] = {statusString, c_jhash64((uint8_t *)statusString.data(), statusString.size(), 0)};
    }

    const auto dbPath = makeDbPath();
    _database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    _database.setDatabaseName(dbPath);

    if (!_database.open()) {
        qCDebug(lcClientStatusReporting) << "Could not setup client reporting, database connection error.";
        return;
    }

    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS clientstatusreporting("
        "name VARCHAR(4096) PRIMARY KEY,"
        "status INTEGER(8),"
        "count INTEGER,"
        "lastOccurrence INTEGER(8))"));
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReporting) << "Could not setup client clientstatusreporting table:" << query.lastError().text();
        return;
    }

    if (!query.prepare(QStringLiteral("CREATE INDEX IF NOT EXISTS name ON clientstatusreporting(name);")) || !query.exec()) {
        qCDebug(lcClientStatusReporting) << "Could not create index on clientstatusreporting table:" << query.lastError().text();
        return;
    }

    if (!query.prepare(QStringLiteral("CREATE TABLE IF NOT EXISTS keyvalue(key VARCHAR(4096), value VARCHAR(4096), PRIMARY KEY(key))")) || !query.exec()) {
        qCDebug(lcClientStatusReporting) << "Could not setup client keyvalue table:" << query.lastError().text();
        return;
    }

    // prevent issues in case enum gets changed in future, hash its value and clean the db in case there was a change
    QByteArray statusNamesContatenated;
    for (int i = 0; i < ClientStatusReporting::Status::Count; ++i) {
        statusNamesContatenated += statusStringFromNumber(static_cast<Status>(i));
    }
    statusNamesContatenated += QByteArray::number(ClientStatusReporting::Status::Count);
    const auto statusNamesHashCurrent = QCryptographicHash::hash(statusNamesContatenated, QCryptographicHash::Md5).toHex();
    const auto statusNamesHashFromDb = getStatusNamesHash();

    if (statusNamesHashCurrent != statusNamesHashFromDb) {
        deleteClientStatusReportingRecords();
        setStatusNamesHash(statusNamesHashCurrent);
    }
    //

    _clientStatusReportingSendTimer.setInterval(clientStatusReportingTrySendTimerInterval);
    connect(&_clientStatusReportingSendTimer, &QTimer::timeout, this, &ClientStatusReporting::sendReportToServer);
    _clientStatusReportingSendTimer.start();

    _isInitialized = true;
}

QVector<ClientStatusReportingRecord> ClientStatusReporting::getClientStatusReportingRecords() const
{
    QVector<ClientStatusReportingRecord> records;

    QMutexLocker locker(&_mutex);

    QSqlQuery query;
    if (!query.prepare(QStringLiteral("SELECT * FROM clientstatusreporting")) || !query.exec()) {
        qCDebug(lcClientStatusReporting) << "Could not get records from clientstatusreporting:" << query.lastError().text();
        return records;
    }

    while (query.next()) {
        ClientStatusReportingRecord record;
        record._status = query.value(query.record().indexOf(QStringLiteral("status"))).toLongLong();
        record._name = query.value(query.record().indexOf(QStringLiteral("name"))).toByteArray();
        record._numOccurences = query.value(query.record().indexOf(QStringLiteral("count"))).toLongLong();
        record._lastOccurence = query.value(query.record().indexOf(QStringLiteral("lastOccurrence"))).toLongLong();
        records.push_back(record);
    }
    return records;
}

void ClientStatusReporting::deleteClientStatusReportingRecords() const
{
    QSqlQuery query;
    if (!query.prepare(QStringLiteral("DELETE FROM clientstatusreporting")) || !query.exec()) {
        qCDebug(lcClientStatusReporting) << "Could not delete records from clientstatusreporting:" << query.lastError().text();
    }
}

Result<void, QString> ClientStatusReporting::setClientStatusReportingRecord(const ClientStatusReportingRecord &record) const
{
    Q_ASSERT(record.isValid());
    if (!record.isValid()) {
        qCDebug(lcClientStatusReporting) << "Failed to set ClientStatusReportingRecord";
        return {QStringLiteral("Invalid parameter")};
    }

    const auto recordCopy = record;

    QMutexLocker locker(&_mutex);

    QSqlQuery query;

    const auto prepareResult = query.prepare(
        QStringLiteral("INSERT OR REPLACE INTO clientstatusreporting (name, status, count, lastOccurrence) VALUES(:name, :status, :count, :lastOccurrence) ON CONFLICT(name) "
        "DO UPDATE SET count = count + 1, lastOccurrence = :lastOccurrence;"));
    query.bindValue(QStringLiteral(":name"), recordCopy._name);
    query.bindValue(QStringLiteral(":status"), recordCopy._status);
    query.bindValue(QStringLiteral(":count"), 1);
    query.bindValue(QStringLiteral(":lastOccurrence"), recordCopy._lastOccurence);

    if (!prepareResult || !query.exec()) {
        const auto errorMessage = query.lastError().text();
        qCDebug(lcClientStatusReporting) << "Could not report client status:" << errorMessage;
        return errorMessage;
    }

    return {};
}

void ClientStatusReporting::reportClientStatus(const Status status) const
{
    if (!_isInitialized) {
        qCDebug(lcClientStatusReporting) << "Could not report status. Status reporting is not initialized";
        return;
    }
    Q_ASSERT(status >= 0 && status < Count);
    if (status < 0 || status >= Status::Count) {
        qCDebug(lcClientStatusReporting) << "Trying to report invalid status:" << status;
        return;
    }

    ClientStatusReportingRecord record;
    record._name = _statusNamesAndHashes[status].first;
    record._status = status;
    record._lastOccurence = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    const auto result = setClientStatusReportingRecord(record);
    if (!result.isValid()) {
        qCDebug(lcClientStatusReporting) << "Could not report client status:" << result.error();
    }
}

void ClientStatusReporting::sendReportToServer()
{
    if (!_isInitialized) {
        qCWarning(lcClientStatusReporting) << "Could not send report to server. Status reporting is not initialized";
        return;
    }

    const auto lastSentReportTime = getLastSentReportTimestamp();
    if (QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() - lastSentReportTime < repordSendIntervalMs) {
        return;
    }

    const auto report = prepareReport();
    if (report.isEmpty()) {
        qCDebug(lcClientStatusReporting) << "Failed to generate report. Report is empty.";
        return;
    }

    const auto clientStatusReportingJob = new JsonApiJob(_account->sharedFromThis(), QStringLiteral("ocs/v2.php/apps/security_guard/diagnostics"));
    clientStatusReportingJob->setBody(QJsonDocument::fromVariant(report));
    clientStatusReportingJob->setVerb(SimpleApiJob::Verb::Put);
    connect(clientStatusReportingJob, &JsonApiJob::jsonReceived, [this](const QJsonDocument &json, int statusCode) {
        if (statusCode == 0 || statusCode == 200 || statusCode == 201 || statusCode == 204) {
            const auto metaFromJson = json.object().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("meta")).toObject();
            const auto codeFromJson = metaFromJson.value(QStringLiteral("statuscode")).toInt();
            if (codeFromJson == 0 || codeFromJson == 200 || codeFromJson == 201 || codeFromJson == 204) {
                reportToServerSentSuccessfully();
                return;
            }
            qCDebug(lcClientStatusReporting) << "Received error when sending client report statusCode:" << statusCode << "codeFromJson:" << codeFromJson;
        }
    });
    clientStatusReportingJob->start();
}

void ClientStatusReporting::reportToServerSentSuccessfully()
{
    deleteClientStatusReportingRecords();
    setLastSentReportTimestamp(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
}

QString ClientStatusReporting::makeDbPath() const
{
    if (!dbPathForTesting.isEmpty()) {
        return dbPathForTesting;
    }
    const auto databaseId = QStringLiteral("%1@%2").arg(_account->davUser(), _account->url().toString());
    const auto databaseIdHash = QCryptographicHash::hash(databaseId.toUtf8(), QCryptographicHash::Md5);

    return ConfigFile().configPath() + QStringLiteral(".userdata_%1.db").arg(QString::fromLatin1(databaseIdHash.left(6).toHex()));
}

quint64 ClientStatusReporting::getLastSentReportTimestamp() const
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral("SELECT value FROM keyvalue WHERE key = (:key)"));
    query.bindValue(QStringLiteral(":key"), lastSentReportTimestamp);
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReporting) << "Could not get last sent report timestamp from keyvalue table. No such record:" << lastSentReportTimestamp;
        return 0;
    }
    if (!query.next()) {
        qCDebug(lcClientStatusReporting) << "Could not get last sent report timestamp from keyvalue table:" << query.lastError().text();
        return 0;
    }
    return query.value(query.record().indexOf(QStringLiteral("value"))).toULongLong();
}

void ClientStatusReporting::setStatusNamesHash(const QByteArray &hash) const
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral("INSERT OR REPLACE INTO keyvalue (key, value) VALUES(:key, :value);"));
    query.bindValue(QStringLiteral(":key"), statusNamesHash);
    query.bindValue(QStringLiteral(":value"), hash);
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReporting) << "Could not set status names hash.";
        return;
    }
}

QByteArray ClientStatusReporting::getStatusNamesHash() const
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral("SELECT value FROM keyvalue WHERE key = (:key)"));
    query.bindValue(QStringLiteral(":key"), statusNamesHash);
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReporting) << "Could not get status names hash. No such record:" << statusNamesHash;
        return {};
    }
    if (!query.next()) {
        qCDebug(lcClientStatusReporting) << "Could not get status names hash:" << query.lastError().text();
        return {};
    }
    return query.value(query.record().indexOf(QStringLiteral("value"))).toByteArray();
}

QVariantMap ClientStatusReporting::prepareReport() const
{
    const auto records = getClientStatusReportingRecords();
    if (records.isEmpty()) {
        return {};
    }

    QVariantMap report;
    report[QStringLiteral("sync_conflicts")] = QVariantMap{};
    report[QStringLiteral("problems")] = QVariantMap{};
    report[QStringLiteral("virus_detected")] = QVariantMap{};
    report[QStringLiteral("e2e_errors")] = QVariantMap{};

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
            problems[record._name] = QVariantMap{{QStringLiteral("count"), record._numOccurences}, {QStringLiteral("oldest"), record._lastOccurence}};
            report[categoryKey] = problems;
        }
    }
    return report;
}

void ClientStatusReporting::setLastSentReportTimestamp(const quint64 timestamp) const
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral("INSERT OR REPLACE INTO keyvalue (key, value) VALUES(:key, :value);"));
    query.bindValue(QStringLiteral(":key"), lastSentReportTimestamp);
    query.bindValue(QStringLiteral(":value"), timestamp);
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReporting) << "Could not set last sent report timestamp from keyvalue table. No such record:" << lastSentReportTimestamp;
        return;
    }
}

QByteArray ClientStatusReporting::statusStringFromNumber(const Status status)
{
    Q_ASSERT(status >= 0 && status < Count);
    if (status < 0 || status >= Status::Count) {
        qCDebug(lcClientStatusReporting) << "Invalid status:" << status;
        return {};
    }

    switch (status) {
    case DownloadError_Cannot_Create_File:
        return QByteArrayLiteral("DownloadError.CANNOT_CREATE_FILE");
    case DownloadError_Conflict:
        return QByteArrayLiteral("DownloadError.CONFLICT");
    case DownloadError_ConflictCaseClash:
        return QByteArrayLiteral("DownloadError.CONFLICT_CASECLASH");
    case DownloadError_ConflictInvalidCharacters:
        return QByteArrayLiteral("DownloadError.CONFLICT_INVALID_CHARACTERS");
    case DownloadError_No_Free_Space:
        return QByteArrayLiteral("DownloadError.NO_FREE_SPACE");
    case DownloadError_ServerError:
        return QByteArrayLiteral("DownloadError.SERVER_ERROR");
    case DownloadError_Virtual_File_Hydration_Failure:
        return QByteArrayLiteral("DownloadError.VIRTUAL_FILE_HYDRATION_FAILURE ");
    case UploadError_Conflict:
        return QByteArrayLiteral("UploadError.CONFLICT_CASECLASH");
    case UploadError_ConflictInvalidCharacters:
        return QByteArrayLiteral("UploadError.CONFLICT_INVALID_CHARACTERS");
    case UploadError_No_Free_Space:
        return QByteArrayLiteral("UploadError.NO_FREE_SPACE");
    case UploadError_No_Write_Permissions:
        return QByteArrayLiteral("UploadError.NO_WRITE_PERMISSIONS");
    case UploadError_ServerError:
        return QByteArrayLiteral("UploadError.SERVER_ERROR");
    case Count:
        return {};
    };
    return {};
}

QByteArray ClientStatusReporting::classifyStatus(const Status status)
{
    Q_ASSERT(status >= 0 && status < Count);
    if (status < 0 || status >= Status::Count) {
        qCDebug(lcClientStatusReporting) << "Invalid status:" << status;
        return {};
    }

    switch (status) {
    case DownloadError_Conflict:
    case DownloadError_ConflictCaseClash:
    case DownloadError_ConflictInvalidCharacters:
    case UploadError_Conflict:
    case UploadError_ConflictInvalidCharacters:
        return QByteArrayLiteral("sync_conflicts");
    case DownloadError_Cannot_Create_File:
    case DownloadError_No_Free_Space:
    case DownloadError_ServerError:
    case DownloadError_Virtual_File_Hydration_Failure:
    case UploadError_No_Free_Space:
    case UploadError_No_Write_Permissions:
    case UploadError_ServerError:
        return QByteArrayLiteral("problems");
    case Count:
        return {};
    };
    return {};
}
int ClientStatusReporting::clientStatusReportingTrySendTimerInterval = 1000 * 60 * 2; // check if the time has come, every 2 minutes
quint64 ClientStatusReporting::repordSendIntervalMs = 24 * 60 * 60 * 1000; // once every 24 hours
QString ClientStatusReporting::dbPathForTesting;
}
