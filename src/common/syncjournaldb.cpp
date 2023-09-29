/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "common/syncjournaldb.h"

#include "common/asserts.h"
#include "common/c_jhash.h"
#include "common/checksums.h"
#include "common/filesystembase.h"
#include "common/preparedsqlquerymanager.h"
#include "common/version.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLoggingCategory>
#include <QStringList>
#include <QUrl>

#include <sqlite3.h>

#include <cstring>

Q_LOGGING_CATEGORY(lcDb, "sync.database", QtInfoMsg)

// SQL expression to check whether path.startswith(prefix + '/')
// Note: '/' + 1 == '0'
#define IS_PREFIX_PATH_OF(prefix, path) \
    "(" path " > (" prefix "||'/') AND " path " < (" prefix "||'0'))"
#define IS_PREFIX_PATH_OR_EQUAL(prefix, path) \
    "(" path " == " prefix " OR " IS_PREFIX_PATH_OF(prefix, path) ")"

namespace {
// base query used to select file record objects, used in combination with WHERE statements.
const auto getFileRecordQueryC = QByteArrayLiteral("SELECT path, inode, modtime, type, md5, fileid, remotePerm, filesize,"
                                                   " ignoredChildrenRemote, contentchecksumtype.name || ':' || contentChecksum,"
                                                   " hasDirtyPlaceholder"
                                                   " FROM metadata"
                                                   " LEFT JOIN checksumtype as contentchecksumtype ON metadata.contentChecksumTypeId == contentchecksumtype.id ");


void fillFileRecordFromGetQuery(OCC::SyncJournalFileRecord &rec, OCC::SqlQuery &query)
{
    rec._path = query.baValue(0);
    rec._inode = query.int64Value(1);
    rec._modtime = query.int64Value(2);
    rec._type = static_cast<ItemType>(query.intValue(3));
    rec._etag = query.baValue(4);
    rec._fileId = query.baValue(5);
    rec._remotePerm = OCC::RemotePermissions::fromDbValue(query.baValue(6));
    rec._fileSize = query.int64Value(7);
    rec._serverHasIgnoredFiles = (query.intValue(8) > 0);
    rec._checksumHeader = query.baValue(9);
}

QByteArray defaultJournalMode(const QString &dbPath)
{
#if defined(Q_OS_WIN)
    // See #2693: Some exFAT file systems seem unable to cope with the
    // WAL journaling mode. They work fine with DELETE.
    QString fileSystem = OCC::FileSystem::fileSystemForPath(dbPath);
    qCInfo(lcDb) << "Detected filesystem" << fileSystem << "for" << dbPath;
    if (fileSystem.contains(QLatin1String("FAT"))) {
        qCInfo(lcDb) << "Filesystem contains FAT - using DELETE journal mode";
        return "DELETE";
    }
#elif defined(Q_OS_MAC)
    if (dbPath.startsWith(QLatin1String("/Volumes/"))) {
        qCInfo(lcDb) << "Mounted sync dir, do not use WAL for" << dbPath;
        return "DELETE";
    }
#else
    Q_UNUSED(dbPath)
#endif
    return "WAL";
}
}

namespace OCC {

SyncJournalDb::SyncJournalDb(const QString &dbFilePath, QObject *parent)
    : QObject(parent)
    , _dbFile(dbFilePath)
    , _transaction(0)
    , _metadataTableIsEmpty(false)
{
    // Allow forcing the journal mode for debugging
    static QByteArray envJournalMode = qgetenv("OWNCLOUD_SQLITE_JOURNAL_MODE");
    _journalMode = envJournalMode;
    if (_journalMode.isEmpty()) {
        _journalMode = defaultJournalMode(_dbFile);
    }
}

QString SyncJournalDb::makeDbName(const QString &localPath,
    const QString &infix)
{
    const QString journalPath = QStringLiteral(".sync_") + infix + QStringLiteral(".db");

    // If it exists already, the path is clearly usable
    QFile file(QDir(localPath).filePath(journalPath));
    if (file.exists()) {
        return journalPath;
    }

    // Try to create a file there
    if (file.open(QIODevice::ReadWrite)) {
        // Ok, all good.
        file.close();
        file.remove();
        return journalPath;
    }

    // Error during creation, just keep the original and throw errors later
    qCWarning(lcDb) << "Could not find a writable database path" << file.fileName() << file.errorString();
    return journalPath;
}

bool SyncJournalDb::dbIsTooNewForClient(const QString &dbFilePath)
{
    SqlDatabase db;
    if (!db.openReadOnly(dbFilePath)) {
        // No DB, we're not downgrading, so we're fine
        return false;
    }

    SqlQuery versionQuery("SELECT major, minor, patch FROM version;", db);
    if (!versionQuery.next().hasData) {
        // Broken DB? Assume we're not upgrading.
        return false;
    }

    int dbMajor = versionQuery.intValue(0);
    //    int minor = versionQuery.intValue(1);
    //    int patch = versionQuery.intValue(2);

    return dbMajor > OCC::Version::versionWithBuildNumber().majorVersion();
}

bool SyncJournalDb::exists()
{
    QMutexLocker locker(&_mutex);
    return (!_dbFile.isEmpty() && QFile::exists(_dbFile));
}

QString SyncJournalDb::databaseFilePath() const
{
    return _dbFile;
}

// Note that this does not change the size of the -wal file, but it is supposed to make
// the normal .db faster since the changes from the wal will be incorporated into it.
// Then the next sync (and the SocketAPI) will have a faster access.
void SyncJournalDb::walCheckpoint()
{
    QElapsedTimer t;
    t.start();
    SqlQuery pragma1(_db);
    pragma1.prepare("PRAGMA wal_checkpoint(FULL);");
    if (pragma1.exec()) {
        qCDebug(lcDb) << "took" << t.elapsed() << "msec";
    }
}

void SyncJournalDb::startTransaction()
{
    if (_transaction == 0) {
        if (!_db.transaction()) {
            qCWarning(lcDb) << "ERROR starting transaction:" << _db.error();
            return;
        }
        _transaction = 1;
    } else {
        qCDebug(lcDb) << "Database Transaction is running, not starting another one!";
    }
}

void SyncJournalDb::commitTransaction()
{
    if (_transaction == 1) {
        if (!_db.commit()) {
            qCWarning(lcDb) << "ERROR committing to the database:" << _db.error();
            return;
        }
        _transaction = 0;
    } else {
        qCDebug(lcDb) << "No database Transaction to commit";
    }
}

bool SyncJournalDb::sqlFail(const QString &log, const SqlQuery &query)
{
    commitTransaction();
    qCWarning(lcDb) << "SQL Error" << log << query.error();
    _db.close();
    OC_ASSERT(false);
    return false;
}

bool SyncJournalDb::checkConnect()
{
    if (_closed) {
        qCWarning(lcDb) << Q_FUNC_INFO << "after the db was closed";
        return false;
    }
    if (autotestFailCounter >= 0) {
        if (!autotestFailCounter--) {
            qCInfo(lcDb) << "Error Simulated";
            return false;
        }
    }

    if (_db.isOpen()) {
        return true;
    }

    if (_dbFile.isEmpty()) {
        qCWarning(lcDb) << "Database filename" << _dbFile << "is empty";
        return false;
    }

    // The database file is created by this call (SQLITE_OPEN_CREATE)
    if (!_db.openOrCreateReadWrite(_dbFile)) {
        QString error = _db.error();
        qCWarning(lcDb) << "Error opening the db:" << error;
        return false;
    }

    if (!QFile::exists(_dbFile)) {
        qCWarning(lcDb) << "Database file" << _dbFile << "does not exist";
        return false;
    }

    SqlQuery pragma1(_db);
    pragma1.prepare("SELECT sqlite_version();");
    if (!pragma1.exec()) {
        return sqlFail(QStringLiteral("SELECT sqlite_version()"), pragma1);
    } else {
        pragma1.next();
        qCInfo(lcDb) << "sqlite3 version" << pragma1.stringValue(0);
    }

    // Set locking mode to avoid issues with WAL on Windows
    static QByteArray locking_mode_env = qgetenv("OWNCLOUD_SQLITE_LOCKING_MODE");
    if (locking_mode_env.isEmpty())
        locking_mode_env = "EXCLUSIVE";
    pragma1.prepare("PRAGMA locking_mode=" + locking_mode_env + ";");
    if (!pragma1.exec()) {
        return sqlFail(QStringLiteral("Set PRAGMA locking_mode"), pragma1);
    } else {
        pragma1.next();
        qCInfo(lcDb) << "sqlite3 locking_mode=" << pragma1.stringValue(0);
    }

    pragma1.prepare("PRAGMA journal_mode=" + _journalMode + ";");
    if (!pragma1.exec()) {
        return sqlFail(QStringLiteral("Set PRAGMA journal_mode"), pragma1);
    } else {
        pragma1.next();
        qCInfo(lcDb) << "sqlite3 journal_mode=" << pragma1.stringValue(0);
    }

    // For debugging purposes, allow temp_store to be set
    static QByteArray env_temp_store = qgetenv("OWNCLOUD_SQLITE_TEMP_STORE");
    if (!env_temp_store.isEmpty()) {
        pragma1.prepare("PRAGMA temp_store = " + env_temp_store + ";");
        if (!pragma1.exec()) {
            return sqlFail(QStringLiteral("Set PRAGMA temp_store"), pragma1);
        }
        qCInfo(lcDb) << "sqlite3 with temp_store =" << env_temp_store;
    }

    // With WAL journal the NORMAL sync mode is safe from corruption,
    // otherwise use the standard FULL mode.
    QByteArray synchronousMode = "FULL";
    if (QString::fromUtf8(_journalMode).compare(QStringLiteral("wal"), Qt::CaseInsensitive) == 0)
        synchronousMode = "NORMAL";
    pragma1.prepare("PRAGMA synchronous = " + synchronousMode + ";");
    if (!pragma1.exec()) {
        return sqlFail(QStringLiteral("Set PRAGMA synchronous"), pragma1);
    } else {
        qCInfo(lcDb) << "sqlite3 synchronous=" << synchronousMode;
    }

    pragma1.prepare("PRAGMA case_sensitive_like = ON;");
    if (!pragma1.exec()) {
        return sqlFail(QStringLiteral("Set PRAGMA case_sensitivity"), pragma1);
    }

    {
        // Future version of the client (2.6) will have an index 'metadata_parent' which
        // depends on a custom sqlite function which does not exist yet in 2.5.
        // So make sure to remove the index if it exists, otherwise we will crash when inserting
        // rows in the metadata database.
        // The 2.6 client will anyway re-creates this index if it does not exist.
        SqlQuery query(_db);
        query.prepare("DROP INDEX IF EXISTS metadata_parent;");
        if (!query.exec()) {
            return sqlFail(QStringLiteral("updateMetadataTableStructure: remove index metadata_parent"), query);
        }
    }

    sqlite3_create_function(_db.sqliteDb(), "parent_hash", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
                                [] (sqlite3_context *ctx,int, sqlite3_value **argv) {
                                    auto text = reinterpret_cast<const char*>(sqlite3_value_text(argv[0]));
                                    const char *end = std::strrchr(text, '/');
                                    if (!end) end = text;
                                    sqlite3_result_int64(ctx, c_jhash64(reinterpret_cast<const uint8_t*>(text),
                                                                        end - text, 0));
                                }, nullptr, nullptr);

    /* Because insert is so slow, we do everything in a transaction, and only need one call to commit */
    startTransaction();

    SqlQuery createQuery(_db);
    createQuery.prepare("CREATE TABLE IF NOT EXISTS metadata("
                        "phash INTEGER(8),"
                        "pathlen INTEGER,"
                        "path VARCHAR(4096),"
                        "inode INTEGER,"
                        "uid INTEGER,"
                        "gid INTEGER,"
                        "mode INTEGER,"
                        "modtime INTEGER(8),"
                        "type INTEGER,"
                        "md5 VARCHAR(32)," /* This is the etag.  Called md5 for compatibility */
                        // updateDatabaseStructure() will add
                        // fileid
                        // remotePerm
                        // filesize
                        // ignoredChildrenRemote
                        // contentChecksum
                        // contentChecksumTypeId
                        // hasDirtyPlaceholder
                        "PRIMARY KEY(phash)"
                        ");");

#ifndef SQLITE_IOERR_SHMMAP
// Requires sqlite >= 3.7.7 but old CentOS6 has sqlite-3.6.20
// Definition taken from https://sqlite.org/c3ref/c_abort_rollback.html
#define SQLITE_IOERR_SHMMAP            (SQLITE_IOERR | (21<<8))
#endif

    if (!createQuery.exec()) {
        // In certain situations the io error can be avoided by switching
        // to the DELETE journal mode, see #5723
        if (_journalMode != "DELETE"
            && createQuery.errorId() == SQLITE_IOERR
            && sqlite3_extended_errcode(_db.sqliteDb()) == SQLITE_IOERR_SHMMAP) {
            qCWarning(lcDb) << "IO error SHMMAP on table creation, attempting with DELETE journal mode";
            _journalMode = "DELETE";
            commitTransaction();
            _db.close();
            return checkConnect();
        }

        return sqlFail(QStringLiteral("Create table metadata"), createQuery);
    }

    createQuery.prepare("CREATE TABLE IF NOT EXISTS downloadinfo("
                        "path VARCHAR(4096),"
                        "tmpfile VARCHAR(4096),"
                        "etag VARCHAR(32),"
                        "errorcount INTEGER,"
                        "PRIMARY KEY(path)"
                        ");");

    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table downloadinfo"), createQuery);
    }


    if (!createUploadInfo()) {
        return false;
    }

    // create the blacklist table.
    createQuery.prepare("CREATE TABLE IF NOT EXISTS blacklist ("
                        "path VARCHAR(4096),"
                        "lastTryEtag VARCHAR[32],"
                        "lastTryModtime INTEGER[8],"
                        "retrycount INTEGER,"
                        "errorstring VARCHAR[4096],"
                        "PRIMARY KEY(path)"
                        ");");

    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table blacklist"), createQuery);
    }

    // create the selectivesync table.
    createQuery.prepare("CREATE TABLE IF NOT EXISTS selectivesync ("
                        "path VARCHAR(4096),"
                        "type INTEGER"
                        ");");

    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table selectivesync"), createQuery);
    }

    // create the checksumtype table.
    createQuery.prepare("CREATE TABLE IF NOT EXISTS checksumtype("
                        "id INTEGER PRIMARY KEY,"
                        "name TEXT UNIQUE"
                        ");");
    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table checksumtype"), createQuery);
    }

    // create the datafingerprint table.
    createQuery.prepare("CREATE TABLE IF NOT EXISTS datafingerprint("
                        "fingerprint TEXT UNIQUE"
                        ");");
    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table datafingerprint"), createQuery);
    }

    // create the flags table.
    createQuery.prepare("CREATE TABLE IF NOT EXISTS flags ("
                        "path TEXT PRIMARY KEY,"
                        "pinState INTEGER"
                        ");");
    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table flags"), createQuery);
    }

    // create the conflicts table.
    createQuery.prepare("CREATE TABLE IF NOT EXISTS conflicts("
                        "path TEXT PRIMARY KEY,"
                        "baseFileId TEXT,"
                        "baseEtag TEXT,"
                        "baseModtime INTEGER"
                        ");");
    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table conflicts"), createQuery);
    }

    createQuery.prepare("CREATE TABLE IF NOT EXISTS version("
                        "major INTEGER(8),"
                        "minor INTEGER(8),"
                        "patch INTEGER(8),"
                        "custom VARCHAR(256)"
                        ");");
    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table version"), createQuery);
    }

    bool forceRemoteDiscovery = false;

    SqlQuery versionQuery("SELECT major, minor, patch FROM version;", _db);
    if (!versionQuery.next().hasData) {
        forceRemoteDiscovery = true;

        createQuery.prepare("INSERT INTO version VALUES (?1, ?2, ?3, ?4);");
        const auto segments = OCC::Version::versionWithBuildNumber().segments();
        for (int i = 0; i < segments.size(); ++i) {
            createQuery.bindValue(i + 1, segments[i]);
        }
        if (!createQuery.exec()) {
            return sqlFail(QStringLiteral("Update version"), createQuery);
        }

    } else {
        int major = versionQuery.intValue(0);
        int minor = versionQuery.intValue(1);
        int patch = versionQuery.intValue(2);

        if (major == 1 && minor == 8 && (patch == 0 || patch == 1)) {
            qCInfo(lcDb) << "possibleUpgradeFromMirall_1_8_0_or_1 detected!";
            forceRemoteDiscovery = true;
        }

        // - There was a bug in versions <2.3.0 that could lead to stale
        //   local files and a remote discovery will fix them.
        //   See #5190 #5242.
        // - New remote HasZSyncMetadata permission added, invalidate cache
        if (major == 2 && minor < 5) {
            qCInfo(lcDb) << "upgrade from client < 2.5.0 detected! forcing remote discovery";
            forceRemoteDiscovery = true;
        }

        // Not comparing the BUILD id here, correct?
        if (QVersionNumber(major, minor, patch) != OCC::Version::version()) {
            createQuery.prepare("UPDATE version SET major=?1, minor=?2, patch =?3, custom=?4 "
                                "WHERE major=?5 AND minor=?6 AND patch=?7;");
            const auto segments = OCC::Version::versionWithBuildNumber().segments();
            for (int i = 0; i < segments.size(); ++i) {
                createQuery.bindValue(i + 1, segments[i]);
            }
            createQuery.bindValue(5, major);
            createQuery.bindValue(6, minor);
            createQuery.bindValue(7, patch);
            if (!createQuery.exec()) {
                return sqlFail(QStringLiteral("Update version"), createQuery);
            }
        }
    }

    commitInternal(QStringLiteral("checkConnect"));

    bool rc = updateDatabaseStructure();
    if (!rc) {
        qCWarning(lcDb) << "Failed to update the database structure!";
    }

    /*
     * If we are upgrading from a client version older than 1.5,
     * we cannot read from the database because we need to fetch the files id and etags.
     *
     *  If 1.8.0 caused missing data in the local tree, so we also don't read from DB
     *  to get back the files that were gone.
     *  In 1.8.1 we had a fix to re-get the data, but this one here is better
     */
    if (forceRemoteDiscovery) {
        forceRemoteDiscoveryNextSyncLocked();
    }
    const auto deleteDownloadInfo = _queryManager.get(PreparedSqlQueryManager::DeleteDownloadInfoQuery, QByteArrayLiteral("DELETE FROM downloadinfo WHERE path=?1"), _db);
    if (!deleteDownloadInfo) {
        return sqlFail(QStringLiteral("prepare _deleteDownloadInfoQuery"), *deleteDownloadInfo);
    }


    const auto deleteUploadInfoQuery = _queryManager.get(PreparedSqlQueryManager::DeleteUploadInfoQuery, QByteArrayLiteral("DELETE FROM uploadinfo WHERE path=?1"), _db);
    if (!deleteUploadInfoQuery) {
        return sqlFail(QStringLiteral("prepare _deleteUploadInfoQuery"), *deleteUploadInfoQuery);
    }

    QByteArray sql("SELECT lastTryEtag, lastTryModtime, retrycount, errorstring, lastTryTime, ignoreDuration, renameTarget, errorCategory, requestId "
                   "FROM blacklist WHERE path=?1");
    if (Utility::fsCasePreserving()) {
        // if the file system is case preserving we have to check the blacklist
        // case insensitively
        sql += " COLLATE NOCASE";
    }
    const auto getErrorBlacklistQuery = _queryManager.get(PreparedSqlQueryManager::GetErrorBlacklistQuery, sql, _db);
    if (!getErrorBlacklistQuery) {
        return sqlFail(QStringLiteral("prepare _getErrorBlacklistQuery"), *getErrorBlacklistQuery);
    }

    // don't start a new transaction now
    commitInternal(QStringLiteral("checkConnect End"), false);

    // This avoid reading from the DB if we already know it is empty
    // thereby speeding up the initial discovery significantly.
    _metadataTableIsEmpty = (getFileRecordCount() == 0);

    // Hide 'em all!
    FileSystem::setFileHidden(databaseFilePath(), true);
    FileSystem::setFileHidden(databaseFilePath() + QStringLiteral("-wal"), true);
    FileSystem::setFileHidden(databaseFilePath() + QStringLiteral("-shm"), true);
    FileSystem::setFileHidden(databaseFilePath() + QStringLiteral("-journal"), true);

    return rc;
}

void SyncJournalDb::close()
{
    QMutexLocker locker(&_mutex);
    qCInfo(lcDb) << "Closing DB" << _dbFile;

    commitTransaction();
    _db.close();
    clearEtagStorageFilter();
    _metadataTableIsEmpty = false;
    _closed = true;
}

void SyncJournalDb::allowReopen()
{
    Q_ASSERT(_closed);
    _closed = false;
}

bool SyncJournalDb::updateDatabaseStructure()
{
    if (!updateMetadataTableStructure())
        return false;
    if (!updateErrorBlacklistTableStructure())
        return false;
    return true;
}

bool SyncJournalDb::updateMetadataTableStructure()
{
    auto columns = tableColumns("metadata");
    bool re = true;

    // check if the file_id column is there and create it if not
    if (columns.isEmpty()) {
        return false;
    }

    if (columns.indexOf("fileid") == -1) {
        SqlQuery query(_db);
        query.prepare("ALTER TABLE metadata ADD COLUMN fileid VARCHAR(128);");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: Add column fileid"), query);
            re = false;
        }

        query.prepare("CREATE INDEX metadata_file_id ON metadata(fileid);");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: create index fileid"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add fileid col"));
    }
    if (columns.indexOf("remotePerm") == -1) {
        SqlQuery query(_db);
        query.prepare("ALTER TABLE metadata ADD COLUMN remotePerm VARCHAR(128);");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: add column remotePerm"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure (remotePerm)"));
    }
    if (columns.indexOf("filesize") == -1) {
        SqlQuery query(_db);
        query.prepare("ALTER TABLE metadata ADD COLUMN filesize BIGINT;");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateDatabaseStructure: add column filesize"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add filesize col"));
    }

    {
        SqlQuery query(_db);
        query.prepare("CREATE INDEX IF NOT EXISTS metadata_inode ON metadata(inode);");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: create index inode"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add inode index"));
    }

    {
        SqlQuery query(_db);
        query.prepare("CREATE INDEX IF NOT EXISTS metadata_path ON metadata(path);");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: create index path"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add path index"));
    }

    {
        SqlQuery query(_db);
        query.prepare("CREATE INDEX IF NOT EXISTS metadata_parent ON metadata(parent_hash(path));");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: create index parent"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add parent index"));
    }

    if (columns.indexOf("ignoredChildrenRemote") == -1) {
        SqlQuery query(_db);
        query.prepare("ALTER TABLE metadata ADD COLUMN ignoredChildrenRemote INT;");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: add ignoredChildrenRemote column"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add ignoredChildrenRemote col"));
    }

    if (columns.indexOf("contentChecksum") == -1) {
        SqlQuery query(_db);
        query.prepare("ALTER TABLE metadata ADD COLUMN contentChecksum TEXT;");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: add contentChecksum column"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add contentChecksum col"));
    }
    if (columns.indexOf("contentChecksumTypeId") == -1) {
        SqlQuery query(_db);
        query.prepare("ALTER TABLE metadata ADD COLUMN contentChecksumTypeId INTEGER;");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: add contentChecksumTypeId column"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add contentChecksumTypeId col"));
    }

    if (columns.indexOf("hasDirtyPlaceholder") == -1) {
        SqlQuery addDirtyQuery(_db);
        addDirtyQuery.prepare("ALTER TABLE metadata ADD COLUMN hasDirtyPlaceholder BOOLEAN;");
        if (!addDirtyQuery.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: add hasDirtyPlaceholder column"), addDirtyQuery);
            re = false;
        }
        SqlQuery dirtyIndex(_db);
        dirtyIndex.prepare("CREATE INDEX hasDirtyPlaceholderIndex ON metadata(path) WHERE hasDirtyPlaceholder=TRUE;");
        if (!dirtyIndex.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: add index on hasDirtyPlaceholder column"), dirtyIndex);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add hasDirtyPlaceholder col"));
    }

    auto uploadInfoColumns = tableColumns("uploadinfo");
    if (uploadInfoColumns.isEmpty())
        return false;
    if (!uploadInfoColumns.contains("contentChecksum")) {
        SqlQuery query(_db);
        query.prepare("ALTER TABLE uploadinfo ADD COLUMN contentChecksum TEXT;");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: add contentChecksum column"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add contentChecksum col for uploadinfo"));
    }

    auto conflictsColumns = tableColumns("conflicts");
    if (conflictsColumns.isEmpty())
        return false;
    if (!conflictsColumns.contains("basePath")) {
        SqlQuery query(_db);
        query.prepare("ALTER TABLE conflicts ADD COLUMN basePath TEXT;");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: add basePath column"), query);
            re = false;
        }
    }

    return re;
}

bool SyncJournalDb::updateErrorBlacklistTableStructure()
{
    auto columns = tableColumns("blacklist");
    bool re = true;

    if (columns.isEmpty()) {
        return false;
    }

    if (columns.indexOf("lastTryTime") == -1) {
        SqlQuery query(_db);
        query.prepare("ALTER TABLE blacklist ADD COLUMN lastTryTime INTEGER(8);");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateBlacklistTableStructure: Add lastTryTime fileid"), query);
            re = false;
        }
        query.prepare("ALTER TABLE blacklist ADD COLUMN ignoreDuration INTEGER(8);");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateBlacklistTableStructure: Add ignoreDuration fileid"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add lastTryTime, ignoreDuration cols"));
    }
    if (columns.indexOf("renameTarget") == -1) {
        SqlQuery query(_db);
        query.prepare("ALTER TABLE blacklist ADD COLUMN renameTarget VARCHAR(4096);");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateBlacklistTableStructure: Add renameTarget"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add renameTarget col"));
    }

    if (columns.indexOf("errorCategory") == -1) {
        SqlQuery query(_db);
        query.prepare("ALTER TABLE blacklist ADD COLUMN errorCategory INTEGER(8);");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateBlacklistTableStructure: Add errorCategory"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add errorCategory col"));
    }

    if (columns.indexOf("requestId") == -1) {
        SqlQuery query(_db);
        query.prepare("ALTER TABLE blacklist ADD COLUMN requestId VARCHAR(36);");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateBlacklistTableStructure: Add requestId"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add errorCategory col"));
    }

    SqlQuery query(_db);
    query.prepare("CREATE INDEX IF NOT EXISTS blacklist_index ON blacklist(path collate nocase);");
    if (!query.exec()) {
        sqlFail(QStringLiteral("updateErrorBlacklistTableStructure: create index blacklit"), query);
        re = false;
    }

    return re;
}

QVector<QByteArray> SyncJournalDb::tableColumns(const QByteArray &table)
{
    QVector<QByteArray> columns;
    if (!checkConnect()) {
        return columns;
    }
    SqlQuery query("PRAGMA table_info('" + table + "');", _db);
    if (!query.exec()) {
        return columns;
    }
    while (query.next().hasData) {
        columns.append(query.baValue(1));
    }
    qCDebug(lcDb) << "Columns in the current journal:" << columns;
    return columns;
}

qint64 SyncJournalDb::getPHash(const QByteArray &file)
{
    qint64 h;
    int len = file.length();

    h = c_jhash64((uint8_t *)file.data(), len, 0);
    return h;
}

Result<void, QString> SyncJournalDb::setFileRecord(const SyncJournalFileRecord &_record)
{
    SyncJournalFileRecord record = _record;
    QMutexLocker locker(&_mutex);

    if (!_etagStorageFilter.isEmpty()) {
        // If we are a directory that should not be read from db next time, don't write the etag
        QByteArray prefix = record._path + "/";
        for (const auto &it : qAsConst(_etagStorageFilter)) {
            if (it.startsWith(prefix)) {
                qCInfo(lcDb) << "Filtered writing the etag of" << prefix << "because it is a prefix of" << it;
                record._etag = "_invalid_";
                break;
            }
        }
    }
    OC_ASSERT(!record._remotePerm.isNull());
    qCInfo(lcDb) << "Updating file record for path:" << record._path << "inode:" << record._inode
                 << "modtime:" << record._modtime << "type:" << record._type
                 << "etag:" << record._etag << "fileId:" << record._fileId << "remotePerm:" << record._remotePerm.toString()
                 << "fileSize:" << record._fileSize << "checksum:" << record._checksumHeader << "hasDirtyPlaceholder:" << record._hasDirtyPlaceholder;

    const qint64 phash = getPHash(record._path);
    if (checkConnect()) {
        int plen = record._path.length();

        QByteArray etag(record._etag);
        if (etag.isEmpty())
            etag = "";
        QByteArray fileId(record._fileId);
        if (fileId.isEmpty())
            fileId = "";
        QByteArray remotePerm = record._remotePerm.toDbValue();

        const auto checksumHeader = ChecksumHeader::parseChecksumHeader(record._checksumHeader);
        int contentChecksumTypeId = mapChecksumType(checksumHeader.type());
        const auto query = _queryManager.get(PreparedSqlQueryManager::SetFileRecordQuery, QByteArrayLiteral("INSERT OR REPLACE INTO metadata "
                                                                                                            "(phash, pathlen, path, inode, uid, gid, mode, modtime, type, md5, fileid, remotePerm, filesize, ignoredChildrenRemote, contentChecksum, contentChecksumTypeId, hasDirtyPlaceholder) "
                                                                                                            "VALUES (?1 , ?2, ?3 , ?4 , ?5 , ?6 , ?7,  ?8 , ?9 , ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17);"),
            _db);
        if (!query) {
            return query->error();
        }

        query->bindValue(1, phash);
        query->bindValue(2, plen);
        query->bindValue(3, record._path);
        query->bindValue(4, record._inode);
        query->bindValue(5, 0); // uid Not used
        query->bindValue(6, 0); // gid Not used
        query->bindValue(7, 0); // mode Not used
        query->bindValue(8, record._modtime);
        query->bindValue(9, record._type);
        query->bindValue(10, etag);
        query->bindValue(11, fileId);
        query->bindValue(12, remotePerm);
        query->bindValue(13, record._fileSize);
        query->bindValue(14, record._serverHasIgnoredFiles ? 1 : 0);
        query->bindValue(15, checksumHeader.checksum());
        query->bindValue(16, contentChecksumTypeId);
        query->bindValue(17, record._hasDirtyPlaceholder);

        if (!query->exec()) {
            return query->error();
        }

        // Can't be true anymore.
        _metadataTableIsEmpty = false;

        return {};
    } else {
        qCWarning(lcDb) << "Failed to connect database.";
        return tr("Failed to connect database."); // checkConnect failed.
    }
}

// TODO: filename -> QBytearray?
bool SyncJournalDb::deleteFileRecord(const QString &filename, bool recursively)
{
    QMutexLocker locker(&_mutex);

    if (checkConnect()) {
        // if (!recursively) {
        // always delete the actual file.

        {
            const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteFileRecordPhash, QByteArrayLiteral("DELETE FROM metadata WHERE phash=?1"), _db);
            if (!query) {
                return false;
            }

            const qint64 phash = getPHash(filename.toUtf8());
            query->bindValue(1, phash);

            if (!query->exec()) {
                return false;
            }
        }

        if (recursively) {
            const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteFileRecordRecursively, QByteArrayLiteral("DELETE FROM metadata WHERE " IS_PREFIX_PATH_OF("?1", "path")), _db);
            if (!query)
                return false;
            query->bindValue(1, filename);
            if (!query->exec()) {
                return false;
            }
        }
        return true;
    } else {
        qCWarning(lcDb) << "Failed to connect database.";
        return false; // checkConnect failed.
    }
}


bool SyncJournalDb::getFileRecord(const QByteArray &filename, SyncJournalFileRecord *rec)
{
    QMutexLocker locker(&_mutex);

    // Reset the output var in case the caller is reusing it.
    Q_ASSERT(rec);
    rec->_path.clear();
    Q_ASSERT(!rec->isValid());

    if (_metadataTableIsEmpty)
        return true; // no error, yet nothing found (rec->isValid() == false)

    if (!checkConnect())
        return false;

    if (!filename.isEmpty()) {
        const auto query = _queryManager.get(PreparedSqlQueryManager::GetFileRecordQuery, getFileRecordQueryC + QByteArrayLiteral("WHERE phash=?1"), _db);
        if (!query) {
            return false;
        }

        query->bindValue(1, getPHash(filename));

        if (!query->exec()) {
            close();
            return false;
        }

        auto next = query->next();
        if (!next.ok) {
            QString err = query->error();
            qCWarning(lcDb) << "No journal entry found for" << filename << "Error:" << err;
            close();
            return false;
        }
        if (next.hasData) {
            fillFileRecordFromGetQuery(*rec, *query);
        }
    }
    return true;
}

bool SyncJournalDb::getFileRecordByInode(quint64 inode, SyncJournalFileRecord *rec)
{
    QMutexLocker locker(&_mutex);

    // Reset the output var in case the caller is reusing it.
    Q_ASSERT(rec);
    rec->_path.clear();
    Q_ASSERT(!rec->isValid());

    if (!inode || _metadataTableIsEmpty)
        return true; // no error, yet nothing found (rec->isValid() == false)

    if (!checkConnect())
        return false;
    const auto query = _queryManager.get(PreparedSqlQueryManager::GetFileRecordQueryByInode, getFileRecordQueryC + QByteArrayLiteral("WHERE inode=?1"), _db);
    if (!query)
        return false;

    query->bindValue(1, inode);

    if (!query->exec())
        return false;

    auto next = query->next();
    if (!next.ok)
        return false;
    if (next.hasData)
        fillFileRecordFromGetQuery(*rec, *query);

    return true;
}

bool SyncJournalDb::getFileRecordsByFileId(const QByteArray &fileId, const std::function<void(const SyncJournalFileRecord &)> &rowCallback)
{
    QMutexLocker locker(&_mutex);

    if (fileId.isEmpty() || _metadataTableIsEmpty)
        return true; // no error, yet nothing found (rec->isValid() == false)

    if (!checkConnect())
        return false;

    const auto query = _queryManager.get(PreparedSqlQueryManager::GetFileRecordQueryByFileId, getFileRecordQueryC + QByteArrayLiteral("WHERE fileid=?1"), _db);
    if (!query) {
        return false;
    }

    query->bindValue(1, fileId);

    if (!query->exec())
        return false;

    forever {
        auto next = query->next();
        if (!next.ok)
            return false;
        if (!next.hasData)
            break;

        SyncJournalFileRecord rec;
        fillFileRecordFromGetQuery(rec, *query);
        rowCallback(rec);
    }

    return true;
}

bool SyncJournalDb::getFilesBelowPath(const QByteArray &path, const std::function<void(const SyncJournalFileRecord&)> &rowCallback)
{
    QMutexLocker locker(&_mutex);

    if (_metadataTableIsEmpty)
        return true; // no error, yet nothing found

    if (!checkConnect())
        return false;

    auto _exec = [&rowCallback](SqlQuery &query) {
        if (!query.exec()) {
            return false;
        }

        forever {
            auto next = query.next();
            if (!next.ok)
                return false;
            if (!next.hasData)
                break;

            SyncJournalFileRecord rec;
            fillFileRecordFromGetQuery(rec, query);
            rowCallback(rec);
        }
        return true;
    };

    if(path.isEmpty()) {
        // Since the path column doesn't store the starting /, the getFilesBelowPathQuery
        // can't be used for the root path "". It would scan for (path > '/' and path < '0')
        // and find nothing. So, unfortunately, we have to use a different query for
        // retrieving the whole tree.

        const auto query = _queryManager.get(PreparedSqlQueryManager::GetAllFilesQuery, getFileRecordQueryC + QByteArrayLiteral("ORDER BY path||'/' ASC"), _db);
        if (!query) {
            return false;
        }
        return _exec(*query);
    } else {
        // This query is used to skip discovery and fill the tree from the
        // database instead
        const auto query = _queryManager.get(PreparedSqlQueryManager::GetFilesBelowPathQuery, getFileRecordQueryC + QByteArrayLiteral("WHERE " IS_PREFIX_PATH_OF("?1", "path")
                                                                                                  // We want to ensure that the contents of a directory are sorted
                                                                                                  // directly behind the directory itself. Without this ORDER BY
                                                                                                  // an ordering like foo, foo-2, foo/file would be returned.
                                                                                                  // With the trailing /, we get foo-2, foo, foo/file. This property
                                                                                                  // is used in fill_tree_from_db().
                                                                                                  " ORDER BY path||'/' ASC"),
            _db);
        if (!query) {
            return false;
        }
        query->bindValue(1, path);
        return _exec(*query);
    }
}

bool SyncJournalDb::listFilesInPath(const QByteArray& path,
                                    const std::function<void (const SyncJournalFileRecord &)>& rowCallback)
{
    QMutexLocker locker(&_mutex);

    if (_metadataTableIsEmpty)
        return true;

    if (!checkConnect())
        return false;

    const auto query = _queryManager.get(PreparedSqlQueryManager::ListFilesInPathQuery, getFileRecordQueryC + QByteArrayLiteral("WHERE parent_hash(path) = ?1 ORDER BY path||'/' ASC"), _db);
    if (!query) {
        return false;
    }
    query->bindValue(1, getPHash(path));

    if (!query->exec())
        return false;

    forever {
        auto next = query->next();
        if (!next.ok)
            return false;
        if (!next.hasData)
            break;

        SyncJournalFileRecord rec;
        fillFileRecordFromGetQuery(rec, *query);
        if (!rec._path.startsWith(path) || rec._path.indexOf("/", path.size() + 1) > 0) {
            qWarning(lcDb) << "hash collision" << path << rec._path;
            continue;
        }
        rowCallback(rec);
    }

    return true;
}

int SyncJournalDb::getFileRecordCount()
{
    QMutexLocker locker(&_mutex);

    SqlQuery query(_db);
    query.prepare("SELECT COUNT(*) FROM metadata");

    if (!query.exec()) {
        return -1;
    }

    if (query.next().hasData) {
        int count = query.intValue(0);
        return count;
    }

    return -1;
}

bool SyncJournalDb::updateFileRecordChecksum(const QString &filename,
    const QByteArray &contentChecksum,
    CheckSums::Algorithm contentChecksumType)
{
    QMutexLocker locker(&_mutex);

    qCInfo(lcDb) << "Updating file checksum" << filename << contentChecksum << contentChecksumType;

    const qint64 phash = getPHash(filename.toUtf8());
    if (!checkConnect()) {
        qCWarning(lcDb) << "Failed to connect database.";
        return false;
    }

    int checksumTypeId = mapChecksumType(contentChecksumType);

    const auto query = _queryManager.get(PreparedSqlQueryManager::SetFileRecordChecksumQuery, QByteArrayLiteral("UPDATE metadata"
                                                                                                                " SET contentChecksum = ?2, contentChecksumTypeId = ?3"
                                                                                                                " WHERE phash == ?1;"),
        _db);
    if (!query) {
        return false;
    }
    query->bindValue(1, phash);
    query->bindValue(2, contentChecksum);
    query->bindValue(3, checksumTypeId);
    return query->exec();
}

Optional<SyncJournalDb::HasHydratedDehydrated> SyncJournalDb::hasHydratedOrDehydratedFiles(const QByteArray &filename)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect())
        return {};

    const auto query = _queryManager.get(PreparedSqlQueryManager::CountDehydratedFilesQuery, QByteArrayLiteral("SELECT DISTINCT type FROM metadata"
                                                                                                               " WHERE (" IS_PREFIX_PATH_OR_EQUAL("?1", "path") " OR ?1 == '');"),
        _db);
    if (!query) {
        return {};
    }

    query->bindValue(1, filename);
    if (!query->exec())
        return {};

    HasHydratedDehydrated result;
    forever {
        auto next = query->next();
        if (!next.ok)
            return {};
        if (!next.hasData)
            break;
        auto type = static_cast<ItemType>(query->intValue(0));
        if (type == ItemTypeFile || type == ItemTypeVirtualFileDehydration)
            result.hasHydrated = true;
        if (type == ItemTypeVirtualFile || type == ItemTypeVirtualFileDownload)
            result.hasDehydrated = true;
    }

    return result;
}

static void toDownloadInfo(SqlQuery &query, SyncJournalDb::DownloadInfo *res)
{
    bool ok = true;
    res->_tmpfile = query.stringValue(0);
    res->_etag = query.baValue(1);
    res->_errorCount = query.intValue(2);
    res->_valid = ok;
}

static bool deleteBatch(SqlQuery &query, const QStringList &entries, const QString &name)
{
    if (entries.isEmpty())
        return true;

    qCDebug(lcDb) << "Removing stale" << name << "entries:" << entries.join(QStringLiteral(", "));
    // FIXME: Was ported from execBatch, check if correct!
    for (const auto &entry : entries) {
        query.reset_and_clear_bindings();
        query.bindValue(1, entry);
        if (!query.exec()) {
            return false;
        }
    }

    return true;
}

SyncJournalDb::DownloadInfo SyncJournalDb::getDownloadInfo(const QString &file)
{
    QMutexLocker locker(&_mutex);

    DownloadInfo res;

    if (checkConnect()) {
        const auto query = _queryManager.get(PreparedSqlQueryManager::GetDownloadInfoQuery, QByteArrayLiteral("SELECT tmpfile, etag, errorcount FROM downloadinfo WHERE path=?1"), _db);
        if (!query) {
            return res;
        }

        query->bindValue(1, file);

        if (!query->exec()) {
            return res;
        }

        if (query->next().hasData) {
            toDownloadInfo(*query, &res);
        }
    }
    return res;
}

void SyncJournalDb::setDownloadInfo(const QString &file, const SyncJournalDb::DownloadInfo &i)
{
    QMutexLocker locker(&_mutex);

    if (!checkConnect()) {
        return;
    }


    if (i._valid) {
        const auto query = _queryManager.get(PreparedSqlQueryManager::SetDownloadInfoQuery, QByteArrayLiteral("INSERT OR REPLACE INTO downloadinfo "
                                                                                                              "(path, tmpfile, etag, errorcount) "
                                                                                                              "VALUES ( ?1 , ?2, ?3, ?4 )"),
            _db);
        if (!query) {
            return;
        }
        query->bindValue(1, file);
        query->bindValue(2, i._tmpfile);
        query->bindValue(3, i._etag);
        query->bindValue(4, i._errorCount);
        query->exec();
    } else {
        const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteDownloadInfoQuery);
        query->bindValue(1, file);
        query->exec();
    }
}

QVector<SyncJournalDb::DownloadInfo> SyncJournalDb::getAndDeleteStaleDownloadInfos(const QSet<QString> &keep)
{
    QVector<SyncJournalDb::DownloadInfo> empty_result;
    QMutexLocker locker(&_mutex);

    if (!checkConnect()) {
        return empty_result;
    }

    QStringList superfluousPaths;
    QVector<SyncJournalDb::DownloadInfo> deleted_entries;
    {
        SqlQuery query(_db);
        // The selected values *must* match the ones expected by toDownloadInfo().
        query.prepare("SELECT tmpfile, etag, errorcount, path FROM downloadinfo");

        if (!query.exec()) {
            return empty_result;
        }

        while (query.next().hasData) {
            const QString file = query.stringValue(3); // path
            if (!keep.contains(file)) {
                superfluousPaths.append(file);
                DownloadInfo info;
                toDownloadInfo(query, &info);
                deleted_entries.append(info);
            }
        }
    }
    {
        const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteDownloadInfoQuery);
        if (!deleteBatch(*query, superfluousPaths, QStringLiteral("downloadinfo"))) {
            return empty_result;
        }
    }

    return deleted_entries;
}

int SyncJournalDb::downloadInfoCount()
{
    int re = 0;

    QMutexLocker locker(&_mutex);
    if (checkConnect()) {
        SqlQuery query("SELECT count(*) FROM downloadinfo", _db);

        if (!query.exec()) {
            sqlFail(QStringLiteral("Count number of downloadinfo entries failed"), query);
        }
        if (query.next().hasData) {
            re = query.intValue(0);
        }
    }
    return re;
}

SyncJournalDb::UploadInfo SyncJournalDb::getUploadInfo(const QString &file)
{
    QMutexLocker locker(&_mutex);


    if (!checkConnect()) {
        return {};
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::GetUploadInfoQuery,
        QByteArrayLiteral("SELECT chunk, transferid, errorcount, size, modtime, contentChecksum, url FROM uploadinfo WHERE path=?1"),
        _db);
    if (!query) {
        return {};
    }
    query->bindValue(1, file);

    if (!query->exec()) {
        return {};
    }

    UploadInfo res;
    if (query->next().hasData) {
        res._path = file;
        res._chunk = query->intValue(0);
        res._transferid = query->intValue(1);
        res._errorCount = query->intValue(2);
        res._size = query->int64Value(3);
        res._modtime = query->int64Value(4);
        res._contentChecksum = query->baValue(5);
        res._url = QUrl::fromEncoded(query->baValue(6));
        res._valid = true;
    }
    return res;
}

std::vector<SyncJournalDb::UploadInfo> SyncJournalDb::getUploadInfos()
{
    QMutexLocker locker(&_mutex);

    const auto query = _queryManager.get(PreparedSqlQueryManager::GetAllUploadInfoQuery,
        QByteArrayLiteral("SELECT chunk, transferid, errorcount, size, modtime, contentChecksum, path, url FROM uploadinfo"),
        _db);
    if (!query) {
        return {};
    }

    if (!query->exec()) {
        return {};
    }

    std::vector<UploadInfo> res;
    while (query->next().hasData) {
        UploadInfo info;
        info._chunk = query->intValue(0);
        info._transferid = query->intValue(1);
        info._errorCount = query->intValue(2);
        info._size = query->int64Value(3);
        info._modtime = query->int64Value(4);
        info._contentChecksum = query->baValue(5);
        info._path = query->stringValue(6);
        info._url = QUrl::fromEncoded(query->baValue(7));
        info._valid = true;
        res.push_back(std::move(info));
    }
    return res;
}

void SyncJournalDb::setUploadInfo(const QString &file, const SyncJournalDb::UploadInfo &i)
{
    QMutexLocker locker(&_mutex);

    if (!checkConnect()) {
        return;
    }

    if (i._valid) {
        const auto query = _queryManager.get(PreparedSqlQueryManager::SetUploadInfoQuery, QByteArrayLiteral("INSERT OR REPLACE INTO uploadinfo "
                                                                                                            "(path, chunk, transferid, errorcount, size, modtime, contentChecksum, url) "
                                                                                                            "VALUES ( ?1 , ?2, ?3 , ?4 ,  ?5, ?6 , ?7, ?8 )"),
            _db);
        if (!query) {
            return;
        }

        query->bindValue(1, file);
        query->bindValue(2, i._chunk);
        query->bindValue(3, i._transferid);
        query->bindValue(4, i._errorCount);
        query->bindValue(5, i._size);
        query->bindValue(6, i._modtime);
        query->bindValue(7, i._contentChecksum);
        query->bindValue(8, i._url.toEncoded());

        if (!query->exec()) {
            return;
        }
    } else {
        const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteUploadInfoQuery);
        query->bindValue(1, file);

        if (!query->exec()) {
            return;
        }
    }
}

QVector<uint> SyncJournalDb::deleteStaleUploadInfos(const QSet<QString> &keep)
{
    QMutexLocker locker(&_mutex);

    if (!checkConnect()) {
        return {};
    }

    SqlQuery query(_db);
    query.prepare("SELECT path,transferid FROM uploadinfo");

    if (!query.exec()) {
        return {};
    }

    QStringList superfluousPaths;
    QVector<uint> ids;

    while (query.next().hasData) {
        const QString file = query.stringValue(0);
        if (!keep.contains(file)) {
            superfluousPaths.append(file);
            ids.append(query.intValue(1));
        }
    }
    qCDebug(lcDb) << Q_FUNC_INFO << "Keep:" << keep << "Removing Stale:" << superfluousPaths;
    const auto deleteUploadInfoQuery = _queryManager.get(PreparedSqlQueryManager::DeleteUploadInfoQuery);
    deleteBatch(*deleteUploadInfoQuery, superfluousPaths, QStringLiteral("uploadinfo"));
    return ids;
}

SyncJournalErrorBlacklistRecord SyncJournalDb::errorBlacklistEntry(const QString &file)
{
    QMutexLocker locker(&_mutex);
    SyncJournalErrorBlacklistRecord entry;

    if (file.isEmpty())
        return entry;

    if (checkConnect()) {
        const auto query = _queryManager.get(PreparedSqlQueryManager::GetErrorBlacklistQuery);
        query->bindValue(1, file);
        if (query->exec()) {
            if (query->next().hasData) {
                entry._lastTryEtag = query->baValue(0);
                entry._lastTryModtime = query->int64Value(1);
                entry._retryCount = query->intValue(2);
                entry._errorString = query->stringValue(3);
                entry._lastTryTime = query->int64Value(4);
                entry._ignoreDuration = query->int64Value(5);
                entry._renameTarget = query->stringValue(6);
                entry._errorCategory = static_cast<SyncJournalErrorBlacklistRecord::Category>(
                    query->intValue(7));
                entry._requestId = query->baValue(8);
                entry._file = file;
            }
        }
    }

    return entry;
}

bool SyncJournalDb::deleteStaleErrorBlacklistEntries(const QSet<QString> &keep)
{
    QMutexLocker locker(&_mutex);

    if (!checkConnect()) {
        return false;
    }

    SqlQuery query(_db);
    query.prepare("SELECT path FROM blacklist");

    if (!query.exec()) {
        return false;
    }

    QStringList superfluousPaths;

    while (query.next().hasData) {
        const QString file = query.stringValue(0);
        if (!keep.contains(file)) {
            superfluousPaths.append(file);
        }
    }

    SqlQuery delQuery(_db);
    delQuery.prepare("DELETE FROM blacklist WHERE path = ?");
    return deleteBatch(delQuery, superfluousPaths, QStringLiteral("blacklist"));
}

void SyncJournalDb::deleteStaleFlagsEntries()
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect())
        return;

    SqlQuery delQuery("DELETE FROM flags WHERE path != '' AND path NOT IN (SELECT path from metadata);", _db);
    delQuery.exec();
}

int SyncJournalDb::errorBlackListEntryCount()
{
    int re = 0;

    QMutexLocker locker(&_mutex);
    if (checkConnect()) {
        SqlQuery query("SELECT count(*) FROM blacklist", _db);

        if (!query.exec()) {
            sqlFail(QStringLiteral("Count number of blacklist entries failed"), query);
        }
        if (query.next().hasData) {
            re = query.intValue(0);
        }
    }
    return re;
}

int SyncJournalDb::wipeErrorBlacklist()
{
    QMutexLocker locker(&_mutex);
    if (checkConnect()) {
        SqlQuery query(_db);

        query.prepare("DELETE FROM blacklist");

        if (!query.exec()) {
            sqlFail(QStringLiteral("Deletion of whole blacklist failed"), query);
            return -1;
        }
        return query.numRowsAffected();
    }
    return -1;
}

void SyncJournalDb::wipeErrorBlacklistEntry(const QString &relativeFile)
{
    if (relativeFile.isEmpty()) {
        return;
    }
    Q_ASSERT(QFileInfo(relativeFile).isRelative());

    QMutexLocker locker(&_mutex);
    if (checkConnect()) {
        SqlQuery query(_db);

        query.prepare("DELETE FROM blacklist WHERE path=?1");
        query.bindValue(1, relativeFile);
        if (!query.exec()) {
            sqlFail(QStringLiteral("Deletion of blacklist item failed."), query);
        }
    }
}

void SyncJournalDb::wipeErrorBlacklistEntry(const QString &relativeFile, SyncJournalErrorBlacklistRecord::Category category)
{
    if (relativeFile.isEmpty()) {
        return;
    }
    Q_ASSERT(QFileInfo(relativeFile).isRelative());

    QMutexLocker locker(&_mutex);
    if (checkConnect()) {
        SqlQuery query(_db);

        query.prepare("DELETE FROM blacklist WHERE path=?1 AND errorCategory=?2");
        query.bindValue(1, relativeFile);
        query.bindValue(2, category);
        if (!query.exec()) {
            sqlFail(QStringLiteral("Deletion of blacklist item failed."), query);
        }
    }
}

void SyncJournalDb::wipeErrorBlacklistCategory(SyncJournalErrorBlacklistRecord::Category category)
{
    QMutexLocker locker(&_mutex);
    if (checkConnect()) {
        SqlQuery query(_db);

        query.prepare("DELETE FROM blacklist WHERE errorCategory=?1");
        query.bindValue(1, category);
        if (!query.exec()) {
            sqlFail(QStringLiteral("Deletion of blacklist category failed."), query);
        }
    }
}

void SyncJournalDb::setErrorBlacklistEntry(const SyncJournalErrorBlacklistRecord &item)
{
    QMutexLocker locker(&_mutex);

    qCInfo(lcDb) << "Setting blacklist entry for" << item._file << item._retryCount
                 << item._errorString << item._lastTryTime << item._ignoreDuration
                 << item._lastTryModtime << item._lastTryEtag << item._renameTarget
                 << item._errorCategory;

    if (!checkConnect()) {
        return;
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::SetErrorBlacklistQuery, QByteArrayLiteral("INSERT OR REPLACE INTO blacklist "
                                                                                                            "(path, lastTryEtag, lastTryModtime, retrycount, errorstring, lastTryTime, ignoreDuration, renameTarget, errorCategory, requestId) "
                                                                                                            "VALUES ( ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)"),
        _db);
    if (!query) {
        return;
    }

    query->bindValue(1, item._file);
    query->bindValue(2, item._lastTryEtag);
    query->bindValue(3, item._lastTryModtime);
    query->bindValue(4, item._retryCount);
    query->bindValue(5, item._errorString);
    query->bindValue(6, item._lastTryTime);
    query->bindValue(7, item._ignoreDuration);
    query->bindValue(8, item._renameTarget);
    query->bindValue(9, item._errorCategory);
    query->bindValue(10, item._requestId);
    query->exec();
}

QSet<QString> SyncJournalDb::getSelectiveSyncList(SyncJournalDb::SelectiveSyncListType type, bool *ok)
{
    QSet<QString> result;
    OC_ASSERT(ok);

    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        *ok = false;
        return result;
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::GetSelectiveSyncListQuery, QByteArrayLiteral("SELECT path FROM selectivesync WHERE type=?1"), _db);
    if (!query) {
        *ok = false;
        return result;
    }

    query->bindValue(1, int(type));
    if (!query->exec()) {
        *ok = false;
        return result;
    }
    forever {
        auto next = query->next();
        if (!next.ok) {
            *ok = false;
            return result;
        }
        if (!next.hasData)
            break;

        auto entry = query->stringValue(0);
        if (!entry.endsWith(QLatin1Char('/'))) {
            entry.append(QLatin1Char('/'));
        }
        result.insert(entry);
    }
    *ok = true;

    return result;
}

void SyncJournalDb::setSelectiveSyncList(SyncJournalDb::SelectiveSyncListType type, const QSet<QString> &list)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return;
    }

    startTransaction();

    //first, delete all entries of this type
    SqlQuery delQuery("DELETE FROM selectivesync WHERE type == ?1", _db);
    delQuery.bindValue(1, int(type));
    if (!delQuery.exec()) {
        qCWarning(lcDb) << "SQL error when deleting selective sync list" << list << delQuery.error();
    }

    SqlQuery insQuery("INSERT INTO selectivesync VALUES (?1, ?2)", _db);
    for (const auto &path : list) {
        insQuery.reset_and_clear_bindings();
        insQuery.bindValue(1, path);
        insQuery.bindValue(2, int(type));
        if (!insQuery.exec()) {
            qCWarning(lcDb) << "SQL error when inserting into selective sync" << type << path << delQuery.error();
        }
    }

    commitInternal(QStringLiteral("setSelectiveSyncList"));
}

void SyncJournalDb::avoidRenamesOnNextSync(const QByteArray &path)
{
    QMutexLocker locker(&_mutex);

    if (!checkConnect()) {
        return;
    }

    SqlQuery query(_db);
    query.prepare("UPDATE metadata SET fileid = '', inode = '0' WHERE " IS_PREFIX_PATH_OR_EQUAL("?1", "path"));
    query.bindValue(1, path);
    query.exec();

    // We also need to remove the ETags so the update phase refreshes the directory paths
    // on the next sync
    schedulePathForRemoteDiscovery(path);
}

void SyncJournalDb::schedulePathForRemoteDiscovery(const QByteArray &fileName)
{
    QMutexLocker locker(&_mutex);

    if (!checkConnect()) {
        return;
    }

    // Remove trailing slash
    auto argument = fileName;
    if (argument.endsWith('/'))
        argument.chop(1);

    SqlQuery query(_db);
    // This query will match entries for which the path is a prefix of fileName
    // Note: ItemTypeDirectory == 2
    query.prepare("UPDATE metadata SET md5='_invalid_' WHERE " IS_PREFIX_PATH_OR_EQUAL("path", "?1") " AND type == 2;");
    query.bindValue(1, argument);
    query.exec();

    // Prevent future overwrite of the etags of this folder and all
    // parent folders for this sync
    argument.append('/');
    _etagStorageFilter.append(argument);
}

void SyncJournalDb::clearEtagStorageFilter()
{
    _etagStorageFilter.clear();
}

void SyncJournalDb::forceRemoteDiscoveryNextSync()
{
    QMutexLocker locker(&_mutex);

    if (!checkConnect()) {
        return;
    }

    forceRemoteDiscoveryNextSyncLocked();
}

void SyncJournalDb::forceRemoteDiscoveryNextSyncLocked()
{
    qCInfo(lcDb) << "Forcing remote re-discovery by deleting folder Etags";
    SqlQuery deleteRemoteFolderEtagsQuery(_db);
    deleteRemoteFolderEtagsQuery.prepare("UPDATE metadata SET md5='_invalid_' WHERE type=2;");
    deleteRemoteFolderEtagsQuery.exec();
}


QByteArray SyncJournalDb::getChecksumType(int checksumTypeId)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return QByteArray();
    }

    // Retrieve the id
    const auto query = _queryManager.get(PreparedSqlQueryManager::GetChecksumTypeQuery, QByteArrayLiteral("SELECT name FROM checksumtype WHERE id=?1"), _db);
    if (!query) {
        return {};
    }
    query->bindValue(1, checksumTypeId);
    if (!query->exec()) {
        return QByteArray();
    }

    if (!query->next().hasData) {
        qCWarning(lcDb) << "No checksum type mapping found for" << checksumTypeId;
        return QByteArray();
    }
    return query->baValue(0);
}

int SyncJournalDb::mapChecksumType(CheckSums::Algorithm checksumType)
{
    if (checksumType == CheckSums::Algorithm::PARSE_ERROR || checksumType == CheckSums::Algorithm::NONE) {
        return 0;
    }

    auto it = _checksymTypeCache.constFind(checksumType);
    if (it != _checksymTypeCache.cend()) {
        return *it;
    }

    // the type is mapped as utf8
    const auto typeName = Utility::enumToString(checksumType).toUtf8();
    // Ensure the checksum type is in the db
    {
        const auto query = _queryManager.get(PreparedSqlQueryManager::InsertChecksumTypeQuery, QByteArrayLiteral("INSERT OR IGNORE INTO checksumtype (name) VALUES (?1)"), _db);
        if (!query) {
            return 0;
        }
        query->bindValue(1, typeName);
        if (!query->exec()) {
            return 0;
        }
    }

    // Retrieve the id
    {
        const auto query = _queryManager.get(PreparedSqlQueryManager::GetChecksumTypeIdQuery, QByteArrayLiteral("SELECT id FROM checksumtype WHERE UPPER(name)=?1"), _db);
        if (!query) {
            return 0;
        }
        query->bindValue(1, typeName);
        if (!query->exec()) {
            return 0;
        }

        if (!query->next().hasData) {
            qCWarning(lcDb) << "No checksum type mapping found for" << checksumType;
            return 0;
        }
        auto value = query->intValue(0);
        _checksymTypeCache[checksumType] = value;
        return value;
    }
}

QByteArray SyncJournalDb::dataFingerprint()
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return QByteArray();
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::GetDataFingerprintQuery, QByteArrayLiteral("SELECT fingerprint FROM datafingerprint"), _db);
    if (!query) {
        return QByteArray();
    }

    if (!query->exec()) {
        return QByteArray();
    }

    if (!query->next().hasData) {
        return QByteArray();
    }
    return query->baValue(0);
}

void SyncJournalDb::setDataFingerprint(const QByteArray &dataFingerprint)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return;
    }

    const auto setDataFingerprintQuery1 = _queryManager.get(PreparedSqlQueryManager::SetDataFingerprintQuery1, QByteArrayLiteral("DELETE FROM datafingerprint;"), _db);
    const auto setDataFingerprintQuery2 = _queryManager.get(PreparedSqlQueryManager::SetDataFingerprintQuery2, QByteArrayLiteral("INSERT INTO datafingerprint (fingerprint) VALUES (?1);"), _db);
    if (!setDataFingerprintQuery1 || !setDataFingerprintQuery2) {
        return;
    }

    setDataFingerprintQuery1->exec();

    setDataFingerprintQuery2->bindValue(1, dataFingerprint);
    setDataFingerprintQuery2->exec();
}

void SyncJournalDb::setConflictRecord(const ConflictRecord &record)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect())
        return;

    const auto query = _queryManager.get(PreparedSqlQueryManager::SetConflictRecordQuery, QByteArrayLiteral("INSERT OR REPLACE INTO conflicts "
                                                                                                            "(path, baseFileId, baseModtime, baseEtag, basePath) "
                                                                                                            "VALUES (?1, ?2, ?3, ?4, ?5);"),
        _db);
    OC_ASSERT(query);
    query->bindValue(1, record.path);
    query->bindValue(2, record.baseFileId);
    query->bindValue(3, record.baseModtime);
    query->bindValue(4, record.baseEtag);
    query->bindValue(5, record.initialBasePath);
    OC_ASSERT(query->exec());
}

ConflictRecord SyncJournalDb::conflictRecord(const QByteArray &path)
{
    ConflictRecord entry;

    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return entry;
    }
    const auto query = _queryManager.get(PreparedSqlQueryManager::GetConflictRecordQuery, QByteArrayLiteral("SELECT baseFileId, baseModtime, baseEtag, basePath FROM conflicts WHERE path=?1;"), _db);
    OC_ASSERT(query);
    query->bindValue(1, path);
    OC_ASSERT(query->exec());
    if (!query->next().hasData)
        return entry;

    entry.path = path;
    entry.baseFileId = query->baValue(0);
    entry.baseModtime = query->int64Value(1);
    entry.baseEtag = query->baValue(2);
    entry.initialBasePath = query->baValue(3);
    return entry;
}

void SyncJournalDb::deleteConflictRecord(const QByteArray &path)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect())
        return;

    const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteConflictRecordQuery, QByteArrayLiteral("DELETE FROM conflicts WHERE path=?1;"), _db);
    OC_ASSERT(query);
    query->bindValue(1, path);
    OC_ASSERT(query->exec());
}

QByteArrayList SyncJournalDb::conflictRecordPaths()
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect())
        return {};

    SqlQuery query(_db);
    query.prepare("SELECT path FROM conflicts");
    OC_ASSERT(query.exec());

    QByteArrayList paths;
    while (query.next().hasData)
        paths.append(query.baValue(0));

    return paths;
}

QByteArray SyncJournalDb::conflictFileBaseName(const QByteArray &conflictName)
{
    auto conflict = conflictRecord(conflictName);
    QByteArray result;
    if (conflict.isValid()) {
        getFileRecordsByFileId(conflict.baseFileId, [&result](const SyncJournalFileRecord &record) {
            if (!record._path.isEmpty())
                result = record._path;
        });
    }

    if (result.isEmpty())
        result = Utility::conflictFileBaseNameFromPattern(conflictName);
    return result;
}

void SyncJournalDb::clearFileTable()
{
    QMutexLocker lock(&_mutex);
    SqlQuery query(_db);
    query.prepare("DELETE FROM metadata;");
    query.exec();
}

void SyncJournalDb::markVirtualFileForDownloadRecursively(const QByteArray &path)
{
    QMutexLocker lock(&_mutex);
    if (!checkConnect())
        return;

    static_assert(ItemTypeVirtualFile == 4 && ItemTypeVirtualFileDownload == 5, "");
    SqlQuery query("UPDATE metadata SET type=5 WHERE "
                   "(" IS_PREFIX_PATH_OF("?1", "path") " OR ?1 == '') "
                   "AND type=4;", _db);
    query.bindValue(1, path);
    query.exec();

    // We also must make sure we do not read the files from the database (same logic as in schedulePathForRemoteDiscovery)
    // This includes all the parents up to the root, but also all the directory within the selected dir.
    static_assert(ItemTypeDirectory == 2, "");
    query.prepare("UPDATE metadata SET md5='_invalid_' WHERE "
                  "(" IS_PREFIX_PATH_OF("?1", "path") " OR ?1 == '' OR " IS_PREFIX_PATH_OR_EQUAL("path", "?1") ") AND type == 2;");
    query.bindValue(1, path);
    query.exec();
}

Optional<PinState> SyncJournalDb::PinStateInterface::rawForPath(const QByteArray &path)
{
    QMutexLocker lock(&_db->_mutex);
    if (!_db->checkConnect())
        return {};

    const auto query = _db->_queryManager.get(PreparedSqlQueryManager::GetRawPinStateQuery, QByteArrayLiteral("SELECT pinState FROM flags WHERE path == ?1;"), _db->_db);
    OC_ASSERT(query);
    query->bindValue(1, path);
    query->exec();

    auto next = query->next();
    if (!next.ok)
        return {};
    // no-entry means Inherited
    if (!next.hasData)
        return PinState::Inherited;

    return static_cast<PinState>(query->intValue(0));
}

Optional<PinState> SyncJournalDb::PinStateInterface::effectiveForPath(const QByteArray &path)
{
    QMutexLocker lock(&_db->_mutex);
    if (!_db->checkConnect())
        return {};

    const auto query = _db->_queryManager.get(PreparedSqlQueryManager::GetEffectivePinStateQuery, QByteArrayLiteral("SELECT pinState FROM flags WHERE"
                                                                                                                    // explicitly allow "" to represent the root path
                                                                                                                    // (it'd be great if paths started with a / and "/" could be the root)
                                                                                                                    " (" IS_PREFIX_PATH_OR_EQUAL("path", "?1") " OR path == '')"
                                                                                                                                                               " AND pinState is not null AND pinState != 0"
                                                                                                                                                               " ORDER BY length(path) DESC LIMIT 1;"),
        _db->_db);
    OC_ASSERT(query);
    query->bindValue(1, path);
    query->exec();

    auto next = query->next();
    if (!next.ok)
        return {};
    // If the root path has no setting, assume Unspecified
    if (!next.hasData) {
        return PinState::Unspecified;
    }

    return static_cast<PinState>(query->intValue(0));
}

Optional<PinState> SyncJournalDb::PinStateInterface::effectiveForPathRecursive(const QByteArray &path)
{
    // Get the item's effective pin state. We'll compare subitem's pin states
    // against this.
    const auto basePin = effectiveForPath(path);
    if (!basePin)
        return {};

    QMutexLocker lock(&_db->_mutex);
    if (!_db->checkConnect())
        return {};

    // Find all the non-inherited pin states below the item
    const auto query = _db->_queryManager.get(PreparedSqlQueryManager::GetSubPinsQuery, QByteArrayLiteral("SELECT DISTINCT pinState FROM flags WHERE"
                                                                                                          " (" IS_PREFIX_PATH_OF("?1", "path") " OR ?1 == '')"
                                                                                                                                               " AND pinState is not null and pinState != 0;"),
        _db->_db);
    OC_ASSERT(query);
    query->bindValue(1, path);
    query->exec();

    // Check if they are all identical
    forever {
        auto next = query->next();
        if (!next.ok)
            return {};
        if (!next.hasData)
            break;
        const auto subPin = static_cast<PinState>(query->intValue(0));
        if (subPin != *basePin)
            return PinState::Inherited;
    }

    return *basePin;
}

void SyncJournalDb::PinStateInterface::setForPath(const QByteArray &path, PinState state)
{
    QMutexLocker lock(&_db->_mutex);
    if (!_db->checkConnect())
        return;

    const auto query = _db->_queryManager.get(PreparedSqlQueryManager::SetPinStateQuery, QByteArrayLiteral(
                                                                                             // If we had sqlite >=3.24.0 everywhere this could be an upsert,
                                                                                             // making further flags columns easy
                                                                                             //"INSERT INTO flags(path, pinState) VALUES(?1, ?2)"
                                                                                             //" ON CONFLICT(path) DO UPDATE SET pinState=?2;"),
                                                                                             // Simple version that doesn't work nicely with multiple columns:
                                                                                             "INSERT OR REPLACE INTO flags(path, pinState) VALUES(?1, ?2);"),
        _db->_db);
    OC_ASSERT(query);
    query->bindValue(1, path);
    query->bindValue(2, state);
    query->exec();
}

void SyncJournalDb::PinStateInterface::wipeForPathAndBelow(const QByteArray &path)
{
    QMutexLocker lock(&_db->_mutex);
    if (!_db->checkConnect())
        return;

    const auto query = _db->_queryManager.get(PreparedSqlQueryManager::WipePinStateQuery, QByteArrayLiteral("DELETE FROM flags WHERE "
                                                                                                            // Allow "" to delete everything
                                                                                                            " (" IS_PREFIX_PATH_OR_EQUAL("?1", "path") " OR ?1 == '');"),
        _db->_db);
    OC_ASSERT(query);
    query->bindValue(1, path);
    query->exec();
}

Optional<QVector<QPair<QByteArray, PinState>>>
SyncJournalDb::PinStateInterface::rawList()
{
    QMutexLocker lock(&_db->_mutex);
    if (!_db->checkConnect())
        return {};

    SqlQuery query("SELECT path, pinState FROM flags;", _db->_db);
    query.exec();

    QVector<QPair<QByteArray, PinState>> result;
    forever {
        auto next = query.next();
        if (!next.ok)
            return {};
        if (!next.hasData)
            break;
        result.append({ query.baValue(0), static_cast<PinState>(query.intValue(1)) });
    }
    return result;
}

SyncJournalDb::PinStateInterface SyncJournalDb::internalPinStates()
{
    return {this};
}

void SyncJournalDb::commit(const QString &context, bool startTrans)
{
    QMutexLocker lock(&_mutex);
    commitInternal(context, startTrans);
}

void SyncJournalDb::commitIfNeededAndStartNewTransaction(const QString &context)
{
    QMutexLocker lock(&_mutex);
    if (_transaction == 1) {
        commitInternal(context, true);
    } else {
        startTransaction();
    }
}

bool SyncJournalDb::open()
{
    QMutexLocker lock(&_mutex);
    return checkConnect();
}

bool SyncJournalDb::isOpen() const
{
    QMutexLocker lock(&_mutex);
    return _db.isOpen();
}

void SyncJournalDb::commitInternal(const QString &context, bool startTrans)
{
    qCDebug(lcDb) << "Transaction commit" << context << (startTrans ? "and starting new transaction" : "");
    commitTransaction();

    if (startTrans) {
        startTransaction();
    }
}

SyncJournalDb::~SyncJournalDb()
{
    close();
}

const QVector<SyncJournalFileRecord> SyncJournalDb::getFileRecordsWithDirtyPlaceholders() const
{
    QMutexLocker locker(&_mutex);

    if (OC_ENSURE(isOpen())) {
        const auto query = _queryManager.get(PreparedSqlQueryManager::GetFileReocrdsWithDirtyPlaceholdersQuery, getFileRecordQueryC + QByteArrayLiteral("WHERE hasDirtyPlaceholder=TRUE"), const_cast<SyncJournalDb *>(this)->_db);
        if (!OC_ENSURE(query)) {
            return {};
        }
        if (!OC_ENSURE(query->exec())) {
            return {};
        }
        QVector<SyncJournalFileRecord> out;
        while (query->next().hasData) {
            SyncJournalFileRecord rec;
            fillFileRecordFromGetQuery(rec, *query);
            out.append(rec);
        }
        return out;
    }
    return {};
}

bool SyncJournalDb::createUploadInfo()
{
    SqlQuery createQuery(QByteArrayLiteral("CREATE TABLE IF NOT EXISTS uploadinfo("
                                           "path VARCHAR(4096),"
                                           "chunk INTEGER,"
                                           "transferid INTEGER,"
                                           "errorcount INTEGER,"
                                           "size INTEGER(8),"
                                           "modtime INTEGER(8),"
                                           "contentChecksum TEXT,"
                                           // uploadUrl TEXT
                                           "PRIMARY KEY(path)"
                                           ");"),
        _db);

    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table uploadinfo"), createQuery);
    }

    const auto columns = tableColumns("uploadinfo");

    if (columns.indexOf(QByteArrayLiteral("url")) == -1) {
        SqlQuery query(QByteArrayLiteral("ALTER TABLE uploadinfo ADD COLUMN url TEXT;"), _db);
        if (!query.exec()) {
            return sqlFail(QStringLiteral("Add column url"), query);
        }
    }
    return true;
}

bool operator==(const SyncJournalDb::DownloadInfo &lhs,
    const SyncJournalDb::DownloadInfo &rhs)
{
    return lhs._errorCount == rhs._errorCount
        && lhs._etag == rhs._etag
        && lhs._tmpfile == rhs._tmpfile
        && lhs._valid == rhs._valid;
}

bool operator==(const SyncJournalDb::UploadInfo &lhs,
    const SyncJournalDb::UploadInfo &rhs)
{
    return lhs._errorCount == rhs._errorCount
        && lhs._chunk == rhs._chunk
        && lhs._modtime == rhs._modtime
        && lhs._valid == rhs._valid
        && lhs._size == rhs._size
        && lhs._transferid == rhs._transferid
        && lhs._contentChecksum == rhs._contentChecksum;
}
} // namespace OCC
