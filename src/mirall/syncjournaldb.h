/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef SYNCJOURNALDB_H
#define SYNCJOURNALDB_H

#include <QObject>
#include <qmutex.h>
#include <QDateTime>
#include <QSqlDatabase>
#include <QHash>
#include <QSqlQuery>

namespace Mirall {
class SyncJournalFileRecord;
class SyncJournalBlacklistRecord;

class SyncJournalDb : public QObject
{
    Q_OBJECT
public:
    explicit SyncJournalDb(const QString& path, QObject *parent = 0);
    virtual ~SyncJournalDb();
    SyncJournalFileRecord getFileRecord( const QString& filename );
    bool setFileRecord( const SyncJournalFileRecord& record );
    bool deleteFileRecord( const QString& filename, bool recursively = false );
    int getFileRecordCount();
    bool exists();

    void updateBlacklistEntry( const SyncJournalBlacklistRecord& item );
    void wipeBlacklistEntry(const QString& file);
    int wipeBlacklist();
    int blackListEntryCount();

    struct DownloadInfo {
        DownloadInfo() : _errorCount(0), _valid(false) {}
        QString _tmpfile;
        QByteArray _etag;
        int _errorCount;
        bool _valid;
    };
    struct UploadInfo {
        UploadInfo() : _chunk(0), _transferid(0), _errorCount(0), _valid(false) {}
        int _chunk;
        int _transferid;
        quint64 _size; //currently unused
        QDateTime _modtime;
        int _errorCount;
        bool _valid;
    };

    DownloadInfo getDownloadInfo(const QString &file);
    void setDownloadInfo(const QString &file, const DownloadInfo &i);
    UploadInfo getUploadInfo(const QString &file);
    void setUploadInfo(const QString &file, const UploadInfo &i);
    SyncJournalBlacklistRecord blacklistEntry( const QString& );

    bool postSyncCleanup( const QHash<QString, QString>& items );

    /* Because sqlite transactions is really slow, we encapsulate everything in big transactions
     * Commit will actually commit the transaction and create a new one.
     */
    void commit(const QString &context, bool startTrans = true);

    void close();


signals:

public slots:

private:
    qint64 getPHash(const QString& ) const;
    bool updateDatabaseStructure();
    bool sqlFail(const QString& log, const QSqlQuery &query );
    void commitInternal(const QString &context, bool startTrans = true);
    void startTransaction();
    void commitTransaction();
    QStringList tableColumns( const QString& table );

    bool checkConnect();
    QSqlDatabase _db;
    QString _dbFile;
    QMutex _mutex; // Public functions are protected with the mutex.
    int _transaction;
    QScopedPointer<QSqlQuery> _getFileRecordQuery;
    QScopedPointer<QSqlQuery> _setFileRecordQuery;
    QScopedPointer<QSqlQuery> _getDownloadInfoQuery;
    QScopedPointer<QSqlQuery> _setDownloadInfoQuery;
    QScopedPointer<QSqlQuery> _deleteDownloadInfoQuery;
    QScopedPointer<QSqlQuery> _getUploadInfoQuery;
    QScopedPointer<QSqlQuery> _setUploadInfoQuery;
    QScopedPointer<QSqlQuery> _deleteUploadInfoQuery;
    QScopedPointer<QSqlQuery> _deleteFileRecordPhash;
    QScopedPointer<QSqlQuery> _deleteFileRecordRecursively;
    QScopedPointer<QSqlQuery> _blacklistQuery;
};

}  // namespace Mirall
#endif // SYNCJOURNALDB_H
