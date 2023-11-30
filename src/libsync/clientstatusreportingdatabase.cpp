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
#include "clientstatusreportingdatabase.h"

#include "account.h"
#include <configfile.h>

#include <QSqlError>
#include <QSqlRecord>
#include <QSqlQuery>

namespace
{
constexpr auto lastSentReportTimestamp = "lastClientStatusReportSentTime";
constexpr auto statusNamesHash = "statusNamesHash";
}

namespace OCC
{
Q_LOGGING_CATEGORY(lcClientStatusReportingDatabase, "nextcloud.sync.clientstatusreportingdatabase", QtInfoMsg)

ClientStatusReportingDatabase::ClientStatusReportingDatabase(const Account *account)
{
    const auto dbPath = makeDbPath(account);
    _database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    _database.setDatabaseName(dbPath);

    if (!_database.open()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not setup client reporting, database connection error.";
        return;
    }

    QSqlQuery query;
    const auto prepareResult =
        query.prepare(QStringLiteral("CREATE TABLE IF NOT EXISTS clientstatusreporting("
                                     "name VARCHAR(4096) PRIMARY KEY,"
                                     "status INTEGER(8),"
                                     "count INTEGER,"
                                     "lastOccurrence INTEGER(8))"));
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not setup client clientstatusreporting table:" << query.lastError().text();
        return;
    }

    if (!query.prepare(QStringLiteral("CREATE TABLE IF NOT EXISTS keyvalue(key VARCHAR(4096), value VARCHAR(4096), PRIMARY KEY(key))")) || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not setup client keyvalue table:" << query.lastError().text();
        return;
    }

    if (!updateStatusNamesHash()) {
        return;
    }

    _isInitialized = true;
}

ClientStatusReportingDatabase::~ClientStatusReportingDatabase()
{
    if (_database.isOpen()) {
        _database.close();
    }
}

QVector<ClientStatusReportingRecord> ClientStatusReportingDatabase::getClientStatusReportingRecords() const
{
    QVector<ClientStatusReportingRecord> records;

    QMutexLocker locker(&_mutex);

    QSqlQuery query;
    if (!query.prepare(QStringLiteral("SELECT * FROM clientstatusreporting")) || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not get records from clientstatusreporting:" << query.lastError().text();
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

Result<void, QString> ClientStatusReportingDatabase::deleteClientStatusReportingRecords() const
{
    QSqlQuery query;
    if (!query.prepare(QStringLiteral("DELETE FROM clientstatusreporting")) || !query.exec()) {
        const auto errorMessage = query.lastError().text();
        qCDebug(lcClientStatusReportingDatabase) << "Could not delete records from clientstatusreporting:" << errorMessage;
        return errorMessage;
    }
    return {};
}

Result<void, QString> ClientStatusReportingDatabase::setClientStatusReportingRecord(const ClientStatusReportingRecord &record) const
{
    Q_ASSERT(record.isValid());
    if (!record.isValid()) {
        qCDebug(lcClientStatusReportingDatabase) << "Failed to set ClientStatusReportingRecord";
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
        qCDebug(lcClientStatusReportingDatabase) << "Could not report client status:" << errorMessage;
        return errorMessage;
    }

    return {};
}

QString ClientStatusReportingDatabase::makeDbPath(const Account *account) const
{
    if (!dbPathForTesting.isEmpty()) {
        return dbPathForTesting;
    }
    const auto databaseId = QStringLiteral("%1@%2").arg(account->davUser(), account->url().toString());
    const auto databaseIdHash = QCryptographicHash::hash(databaseId.toUtf8(), QCryptographicHash::Md5);

    return ConfigFile().configPath() + QStringLiteral(".userdata_%1.db").arg(QString::fromLatin1(databaseIdHash.left(6).toHex()));
}

bool ClientStatusReportingDatabase::updateStatusNamesHash() const
{
    QByteArray statusNamesContatenated;
    for (int i = 0; i < static_cast<int>(ClientStatusReportingStatus::Count); ++i) {
        statusNamesContatenated += clientStatusstatusStringFromNumber(static_cast<ClientStatusReportingStatus>(i));
    }
    statusNamesContatenated += QByteArray::number(static_cast<int>(ClientStatusReportingStatus::Count));
    const auto statusNamesHashCurrent = QCryptographicHash::hash(statusNamesContatenated, QCryptographicHash::Md5).toHex();
    const auto statusNamesHashFromDb = getStatusNamesHash();

    if (statusNamesHashCurrent != statusNamesHashFromDb) {
        auto result = deleteClientStatusReportingRecords();
        if (!result.isValid()) {
            return false;
        }

        result = setStatusNamesHash(statusNamesHashCurrent);
        if (!result.isValid()) {
            return false;
        }
    }
    return true;
}

QVector<QByteArray> ClientStatusReportingDatabase::getTableColumns(const QString &table) const
{
    QVector<QByteArray> columns;
    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral("PRAGMA table_info('%1');").arg(table));
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could get table columns" << query.lastError().text();
        return columns;
    }
    while (query.next()) {
        columns.append(query.value(1).toByteArray());
    }
    return columns;
}

bool ClientStatusReportingDatabase::addColumn(const QString &tableName, const QString &columnName, const QString &dataType, const bool withIndex) const
{
    const auto columns = getTableColumns(tableName);
    const auto latin1ColumnName = columnName.toLatin1();
    if (columns.indexOf(latin1ColumnName) == -1) {
        QSqlQuery query;
        const auto prepareResult = query.prepare(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3;").arg(tableName, columnName, dataType));
        if (!prepareResult || !query.exec()) {
            qCDebug(lcClientStatusReportingDatabase) << QStringLiteral("Failed to update table %1 structure: add %2 column").arg(tableName, columnName) << query.lastError().text();
            return false;
        }

        if (withIndex) {
            const auto prepareResult = query.prepare(QStringLiteral("CREATE INDEX %1_%2 ON %1(%2);").arg(tableName, columnName));
            if (!prepareResult || !query.exec()) {
                qCDebug(lcClientStatusReportingDatabase) << QStringLiteral("Failed to update table %1 structure: create index %2 column").arg(tableName, columnName) << query.lastError().text();
                return false;
            }
        }
    }
    return true;
}

quint64 ClientStatusReportingDatabase::getLastSentReportTimestamp() const
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral("SELECT value FROM keyvalue WHERE key = (:key)"));
    query.bindValue(QStringLiteral(":key"), lastSentReportTimestamp);
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not get last sent report timestamp from keyvalue table. No such record:" << lastSentReportTimestamp;
        return 0;
    }
    if (!query.next()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not get last sent report timestamp from keyvalue table:" << query.lastError().text();
        return 0;
    }
    return query.value(query.record().indexOf(QStringLiteral("value"))).toULongLong();
}

Result<void, QString> ClientStatusReportingDatabase::setStatusNamesHash(const QByteArray &hash) const
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral("INSERT OR REPLACE INTO keyvalue (key, value) VALUES(:key, :value);"));
    query.bindValue(QStringLiteral(":key"), statusNamesHash);
    query.bindValue(QStringLiteral(":value"), hash);
    if (!prepareResult || !query.exec()) {
        const auto errorMessage = query.lastError().text();
        qCDebug(lcClientStatusReportingDatabase) << "Could not set status names hash." << errorMessage;
        return errorMessage;
    }
    return {};
}

QByteArray ClientStatusReportingDatabase::getStatusNamesHash() const
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral("SELECT value FROM keyvalue WHERE key = (:key)"));
    query.bindValue(QStringLiteral(":key"), statusNamesHash);
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not get status names hash. No such record:" << statusNamesHash;
        return {};
    }
    if (!query.next()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not get status names hash:" << query.lastError().text();
        return {};
    }
    return query.value(query.record().indexOf(QStringLiteral("value"))).toByteArray();
}

bool ClientStatusReportingDatabase::isInitialized() const
{
    return _isInitialized;
}

void ClientStatusReportingDatabase::setLastSentReportTimestamp(const quint64 timestamp) const
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral("INSERT OR REPLACE INTO keyvalue (key, value) VALUES(:key, :value);"));
    query.bindValue(QStringLiteral(":key"), lastSentReportTimestamp);
    query.bindValue(QStringLiteral(":value"), timestamp);
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not set last sent report timestamp from keyvalue table. No such record:" << lastSentReportTimestamp;
        return;
    }
}
QString ClientStatusReportingDatabase::dbPathForTesting;
}
