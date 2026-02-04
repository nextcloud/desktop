/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QStringList>
#include <QElapsedTimer>
#include <QUrl>
#include <QDir>
#include <sqlite3.h>
#include <cstring>

#include "common/syncjournaldb.h"
#include "version.h"
#include "filesystembase.h"
#include "common/asserts.h"
#include "common/checksums.h"
#include "common/preparedsqlquerymanager.h"

#include "common/c_jhash.h"

// SQL expression to check whether path.startswith(prefix + '/')
// Note: '/' + 1 == '0'
#define IS_PREFIX_PATH_OF(prefix, path) \
    "(" path " > (" prefix "||'/') AND " path " < (" prefix "||'0'))"
#define IS_PREFIX_PATH_OR_EQUAL(prefix, path) \
    "(" path " == " prefix " OR " IS_PREFIX_PATH_OF(prefix, path) ")"

static constexpr auto MAJOR_VERSION_3 = 3;
static constexpr auto MINOR_VERSION_16 = 16;

using namespace Qt::StringLiterals;

namespace OCC {

Q_LOGGING_CATEGORY(lcDb, "nextcloud.sync.database", QtInfoMsg)

#define GET_FILE_RECORD_QUERY \
        "SELECT path, inode, modtime, type, md5, fileid, remotePerm, filesize," \
        "  ignoredChildrenRemote, contentchecksumtype.name || ':' || contentChecksum, e2eMangledName, isE2eEncrypted, e2eCertificateFingerprint, " \
        "  lock, lockOwnerDisplayName, lockOwnerId, lockType, lockOwnerEditor, lockTime, lockTimeout, lockToken, isShared, lastShareStateFetchedTimestmap, " \
        "  sharedByMe, isLivePhoto, livePhotoFile, quotaBytesUsed, quotaBytesAvailable" \
        " FROM metadata" \
        "  LEFT JOIN checksumtype as contentchecksumtype ON metadata.contentChecksumTypeId == contentchecksumtype.id"

static void fillFileRecordFromGetQuery(SyncJournalFileRecord &rec, SqlQuery &query)
{
    rec._path = query.baValue(0);
    rec._inode = query.int64Value(1);
    rec._modtime = query.int64Value(2);
    rec._type = static_cast<ItemType>(query.intValue(3));
    rec._etag = query.baValue(4);
    rec._fileId = query.baValue(5);
    rec._remotePerm = RemotePermissions::fromDbValue(query.baValue(6));
    rec._fileSize = query.int64Value(7);
    rec._serverHasIgnoredFiles = (query.intValue(8) > 0);
    rec._checksumHeader = query.baValue(9);
    rec._e2eMangledName = query.baValue(10);
    rec._e2eEncryptionStatus = static_cast<SyncJournalFileRecord::EncryptionStatus>(query.intValue(11));
    rec._lockstate._locked = query.intValue(13) > 0;
    rec._lockstate._lockOwnerDisplayName = query.stringValue(14);
    rec._lockstate._lockOwnerId = query.stringValue(15);
    rec._lockstate._lockOwnerType = query.int64Value(16);
    rec._lockstate._lockEditorApp = query.stringValue(17);
    rec._lockstate._lockTime = query.int64Value(18);
    rec._lockstate._lockTimeout = query.int64Value(19);
    rec._lockstate._lockToken = query.stringValue(20);
    rec._isShared = query.intValue(21) > 0;
    rec._lastShareStateFetchedTimestamp = query.int64Value(22);
    rec._sharedByMe = query.intValue(23) > 0;
    rec._isLivePhoto = query.intValue(24) > 0;
    rec._livePhotoFile = query.stringValue(25);
    rec._folderQuota.bytesUsed = query.int64Value(26);
    rec._folderQuota.bytesAvailable = query.int64Value(27);
}

static QByteArray defaultJournalMode(const QString &dbPath)
{
#if defined(Q_OS_WIN)
    // See #2693: Some exFAT file systems seem unable to cope with the
    // WAL journaling mode. They work fine with DELETE.
    QString fileSystem = FileSystem::fileSystemForPath(dbPath);
    qCInfo(lcDb) << "Detected filesystem" << fileSystem << "for" << dbPath;
    if (fileSystem.contains(QLatin1String("FAT"))) {
        qCInfo(lcDb) << "Filesystem contains FAT - using DELETE journal mode";
        return "DELETE";
    }
#elif defined(Q_OS_MACOS)
    if (dbPath.startsWith(QLatin1String("/Volumes/"))) {
        qCInfo(lcDb) << "Mounted sync dir, do not use WAL for" << dbPath;
        return "DELETE";
    }
#else
    Q_UNUSED(dbPath)
#endif
    return "WAL";
}

SyncJournalDb::SyncJournalDb(const QString &dbFilePath, QObject *parent)
    : QObject(parent)
    , _dbFile(dbFilePath)
{
    // Allow forcing the journal mode for debugging
    static QByteArray envJournalMode = qgetenv("OWNCLOUD_SQLITE_JOURNAL_MODE");
    _journalMode = envJournalMode;
    if (_journalMode.isEmpty()) {
        _journalMode = defaultJournalMode(_dbFile);
    }
}

QString SyncJournalDb::makeDbName(const QString &localPath,
    const QUrl &remoteUrl,
    const QString &remotePath,
    const QString &user)
{
    QString journalPath = QStringLiteral(".sync_");

    QString key = QStringLiteral("%1@%2:%3").arg(user, remoteUrl.toString(), remotePath);

    QByteArray ba = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5);
    journalPath += QString::fromLatin1(ba.left(6).toHex()) + QStringLiteral(".db");

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

bool SyncJournalDb::maybeMigrateDb(const QString &localPath, const QString &absoluteJournalPath)
{
    const QString oldDbName = localPath + QLatin1String(".csync_journal.db");
    if (!FileSystem::fileExists(oldDbName)) {
        return true;
    }
    const QString oldDbNameShm = oldDbName + QStringLiteral("-shm");
    const QString oldDbNameWal = oldDbName + QStringLiteral("-wal");

    const QString newDbName = absoluteJournalPath;
    const QString newDbNameShm = newDbName + QStringLiteral("-shm");
    const QString newDbNameWal = newDbName + QStringLiteral("-wal");

    // Whenever there is an old db file, migrate it to the new db path.
    // This is done to make switching from older versions to newer versions
    // work correctly even if the user had previously used a new version
    // and therefore already has an (outdated) new-style db file.
    QString error;

    if (FileSystem::fileExists(newDbName)) {
        if (!FileSystem::remove(newDbName, &error)) {
            qCWarning(lcDb) << "Database migration: Could not remove db file" << newDbName
                            << "due to" << error;
            return false;
        }
    }
    if (FileSystem::fileExists(newDbNameWal)) {
        if (!FileSystem::remove(newDbNameWal, &error)) {
            qCWarning(lcDb) << "Database migration: Could not remove db WAL file" << newDbNameWal
                            << "due to" << error;
            return false;
        }
    }
    if (FileSystem::fileExists(newDbNameShm)) {
        if (!FileSystem::remove(newDbNameShm, &error)) {
            qCWarning(lcDb) << "Database migration: Could not remove db SHM file" << newDbNameShm
                            << "due to" << error;
            return false;
        }
    }

    if (!FileSystem::rename(oldDbName, newDbName, &error)) {
        qCWarning(lcDb) << "Database migration: could not rename" << oldDbName
                        << "to" << newDbName << ":" << error;
        return false;
    }
    if (!FileSystem::rename(oldDbNameWal, newDbNameWal, &error)) {
        qCWarning(lcDb) << "Database migration: could not rename" << oldDbNameWal
                        << "to" << newDbNameWal << ":" << error;
        return false;
    }
    if (!FileSystem::rename(oldDbNameShm, newDbNameShm, &error)) {
        qCWarning(lcDb) << "Database migration: could not rename" << oldDbNameShm
                        << "to" << newDbNameShm << ":" << error;
        return false;
    }

    qCInfo(lcDb) << "Journal successfully migrated from" << oldDbName << "to" << newDbName;
    return true;
}

bool SyncJournalDb::findPathInSelectiveSyncList(const QStringList &list, const QString &path)
{
    Q_ASSERT(std::is_sorted(list.cbegin(), list.cend()));

    if (list.size() == 1 && list.first() == QStringLiteral("/")) {
        // Special case for the case "/" is there, it matches everything
        return true;
    }

    const QString pathSlash = path + QLatin1Char('/');

    // Since the list is sorted, we can do a binary search.
    // If the path is a prefix of another item or right after in the lexical order.
    auto it = std::lower_bound(list.cbegin(), list.cend(), pathSlash);

    if (it != list.cend() && *it == pathSlash) {
        return true;
    }

    if (it == list.cbegin()) {
        return false;
    }
    --it;
    Q_ASSERT(it->endsWith(QLatin1Char('/'))); // Folder::setSelectiveSyncBlackList makes sure of that
    return pathSlash.startsWith(*it);
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
    ASSERT(false);
    return false;
}

bool SyncJournalDb::checkConnect()
{
    if (autotestFailCounter >= 0) {
        if (!autotestFailCounter--) {
            qCInfo(lcDb) << "Error Simulated";
            return false;
        }
    }

    if (_db.isOpen()) {
        // Unfortunately the sqlite isOpen check can return true even when the underlying storage
        // has become unavailable - and then some operations may cause crashes. See #6049
        if (!QFile::exists(_dbFile)) {
            qCWarning(lcDb) << "Database open, but file" << _dbFile << "does not exist";
            close();
            return false;
        }
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

    createQuery.prepare("CREATE TABLE IF NOT EXISTS key_value_store(key VARCHAR(4096), value VARCHAR(4096), PRIMARY KEY(key));");

    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table key_value_store"), createQuery);
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

    createQuery.prepare("CREATE TABLE IF NOT EXISTS uploadinfo("
                        "path VARCHAR(4096),"
                        "chunk INTEGER,"
                        "transferid INTEGER,"
                        "errorcount INTEGER,"
                        "size INTEGER(8),"
                        "modtime INTEGER(8),"
                        "contentChecksum TEXT,"
                        "PRIMARY KEY(path)"
                        ");");

    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table uploadinfo"), createQuery);
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

    createQuery.prepare("CREATE TABLE IF NOT EXISTS async_poll("
                        "path VARCHAR(4096),"
                        "modtime INTEGER(8),"
                        "filesize BIGINT,"
                        "pollpath VARCHAR(4096));");
    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table async_poll"), createQuery);
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

    // create the caseconflicts table.
    createQuery.prepare("CREATE TABLE IF NOT EXISTS caseconflicts("
        "path TEXT PRIMARY KEY,"
        "baseFileId TEXT,"
        "baseEtag TEXT,"
        "baseModtime INTEGER,"
        "basePath TEXT UNIQUE"
        ");");
    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table caseconflicts"), createQuery);
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

     // create the e2EeLockedFolders table.
    createQuery.prepare(
        "CREATE TABLE IF NOT EXISTS e2EeLockedFolders("
        "folderId VARCHAR(128) PRIMARY KEY,"
        "token VARCHAR(4096)"
        ");");
    if (!createQuery.exec()) {
        return sqlFail(QStringLiteral("Create table e2EeLockedFolders"), createQuery);
    }

    bool forceRemoteDiscovery = false;

    SqlQuery versionQuery("SELECT major, minor, patch FROM version;", _db);
    if (!versionQuery.next().hasData) {
        forceRemoteDiscovery = true;

        createQuery.prepare("INSERT INTO version VALUES (?1, ?2, ?3, ?4);");
        createQuery.bindValue(1, MIRALL_VERSION_MAJOR);
        createQuery.bindValue(2, MIRALL_VERSION_MINOR);
        createQuery.bindValue(3, MIRALL_VERSION_PATCH);
        createQuery.bindValue(4, static_cast<qulonglong>(MIRALL_VERSION_BUILD));
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

        // There was a bug in versions <2.3.0 that could lead to stale
        // local files and a remote discovery will fix them.
        // See #5190 #5242.
        if (major == 2 && minor < 3) {
            qCInfo(lcDb) << "upgrade form client < 2.3.0 detected! forcing remote discovery";
            forceRemoteDiscovery = true;
        }

        // Not comparing the BUILD id here, correct?
        if (!(major == MIRALL_VERSION_MAJOR && minor == MIRALL_VERSION_MINOR && patch == MIRALL_VERSION_PATCH)) {
            createQuery.prepare("UPDATE version SET major=?1, minor=?2, patch =?3, custom=?4 "
                                "WHERE major=?5 AND minor=?6 AND patch=?7;");
            createQuery.bindValue(1, MIRALL_VERSION_MAJOR);
            createQuery.bindValue(2, MIRALL_VERSION_MINOR);
            createQuery.bindValue(3, MIRALL_VERSION_PATCH);
            createQuery.bindValue(4, static_cast<qulonglong>(MIRALL_VERSION_BUILD));
            createQuery.bindValue(5, major);
            createQuery.bindValue(6, minor);
            createQuery.bindValue(7, patch);
            if (!createQuery.exec()) {
                return sqlFail(QStringLiteral("Update version"), createQuery);
            }

            if (major < MAJOR_VERSION_3 || (major == MAJOR_VERSION_3 && minor <= MINOR_VERSION_16)) {
                const auto fixEncryptionResult = ensureCorrectEncryptionStatus();
                if (!fixEncryptionResult) {
                    qCWarning(lcDb) << "Failed to update the encryption status";
                }
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
        qCWarning(lcDb) << "database error:" << deleteDownloadInfo->error();
        return sqlFail(QStringLiteral("prepare _deleteDownloadInfoQuery"), *deleteDownloadInfo);
    }


    const auto deleteUploadInfoQuery = _queryManager.get(PreparedSqlQueryManager::DeleteUploadInfoQuery, QByteArrayLiteral("DELETE FROM uploadinfo WHERE path=?1"), _db);
    if (!deleteUploadInfoQuery) {
        qCWarning(lcDb) << "database error:" << deleteUploadInfoQuery->error();
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
        qCWarning(lcDb) << "database error:" << getErrorBlacklistQuery->error();
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
}


bool SyncJournalDb::updateDatabaseStructure()
{
    if (!updateMetadataTableStructure())
        return false;
    if (!updateErrorBlacklistTableStructure())
        return false;
    return true;
}

bool SyncJournalDb::hasDefaultValue(const QString &columnName)
{
    SqlQuery query(_db);
    const auto selectDefault = QStringLiteral("SELECT dflt_value FROM pragma_table_info('metadata') WHERE name = '%1';").arg(columnName);
    query.prepare(selectDefault.toLatin1());
    if (!query.exec()) {
        sqlFail(QStringLiteral("check default value for: %1").arg(columnName), query);
        return false;
    }

    if (const auto result = query.next();!result.ok || !result.hasData) {
        qCWarning(lcDb) << "database error:" << query.error();
        return false;
    }

    return !query.nullValue(0);
}

bool SyncJournalDb::removeColumn(const QString &columnName)
{
    SqlQuery query(_db);
    const auto request = QStringLiteral("ALTER TABLE metadata DROP COLUMN %1;").arg(columnName);
    query.prepare(request.toLatin1());
    if (!query.exec()) {
        sqlFail(QStringLiteral("update metadata structure: drop %1 column").arg(columnName), query);
        return false;
    }

    commitInternal(QStringLiteral("update database structure: drop %1 column").arg(columnName));
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

    const auto columnExists = [&columns] (const QString &columnName) -> bool {
        return columns.indexOf(columnName.toLatin1()) > -1;
    };

    const auto addColumn = [this, &re, &columnExists] (const QString &columnName, const QString &dataType, const bool withIndex = false, const QString defaultCommand = {}) {
        if (!columnExists(columnName)) {
            SqlQuery query(_db);
            auto request = QStringLiteral("ALTER TABLE metadata ADD COLUMN %1 %2").arg(columnName).arg(dataType);
            if (!defaultCommand.isEmpty()) {
                request.append(QStringLiteral(" ") + defaultCommand);
            }
            request.append(QStringLiteral(";"));
            query.prepare(request.toLatin1());
            if (!query.exec()) {
                sqlFail(QStringLiteral("updateMetadataTableStructure: add %1 column").arg(columnName), query);
                re = false;
            }

            if (withIndex) {
                query.prepare(QStringLiteral("CREATE INDEX metadata_%1 ON metadata(%1);").arg(columnName).toLatin1());
                if (!query.exec()) {
                    sqlFail(QStringLiteral("updateMetadataTableStructure: create index %1").arg(columnName), query);
                    re = false;
                }
            }
            commitInternal(QStringLiteral("update database structure: add %1 column").arg(columnName));
        }
    };

    addColumn(QStringLiteral("fileid"), QStringLiteral("VARCHAR(128)"), true);
    addColumn(QStringLiteral("remotePerm"), QStringLiteral("VARCHAR(128)"));
    addColumn(QStringLiteral("filesize"), QStringLiteral("BIGINT"));

    if (true) {
        SqlQuery query(_db);
        query.prepare("CREATE INDEX IF NOT EXISTS metadata_inode ON metadata(inode);");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: create index inode"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add inode index"));
    }

    if (true) {
        SqlQuery query(_db);
        query.prepare("CREATE INDEX IF NOT EXISTS metadata_path ON metadata(path);");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: create index path"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add path index"));
    }

    if (true) {
        SqlQuery query(_db);
        query.prepare("CREATE INDEX IF NOT EXISTS metadata_parent ON metadata(parent_hash(path));");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: create index parent"), query);
            re = false;
        }
        commitInternal(QStringLiteral("update database structure: add parent index"));
    }

    addColumn(QStringLiteral("ignoredChildrenRemote"), QStringLiteral("INT"));
    addColumn(QStringLiteral("contentChecksum"), QStringLiteral("TEXT"));
    addColumn(QStringLiteral("contentChecksumTypeId"), QStringLiteral("INTEGER"));
    addColumn(QStringLiteral("e2eMangledName"), QStringLiteral("TEXT"));
    addColumn(QStringLiteral("isE2eEncrypted"), QStringLiteral("INTEGER"));
    addColumn(QStringLiteral("e2eCertificateFingerprint"), QStringLiteral("TEXT"));
    addColumn(QStringLiteral("isShared"), QStringLiteral("INTEGER"));
    addColumn(QStringLiteral("lastShareStateFetchedTimestmap"), QStringLiteral("INTEGER"));
    addColumn(QStringLiteral("sharedByMe"), QStringLiteral("INTEGER"));

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

    if (true) {
        SqlQuery query(_db);

        query.prepare("CREATE INDEX IF NOT EXISTS metadata_e2e_id ON metadata(e2eMangledName);");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: create index e2eMangledName"), query);
            re = false;
        }

        query.prepare("CREATE INDEX IF NOT EXISTS metadata_e2e_status ON metadata (path, phash, type, isE2eEncrypted)");
        if (!query.exec()) {
            sqlFail(QStringLiteral("updateMetadataTableStructure: create index metadata_e2e_status"), query);
            re = false;
        }

        commitInternal(QStringLiteral("update database structure: add e2eMangledName index"));
    }

    addColumn(QStringLiteral("lock"), QStringLiteral("INTEGER"));
    addColumn(QStringLiteral("lockType"), QStringLiteral("INTEGER"));
    addColumn(QStringLiteral("lockOwnerDisplayName"), QStringLiteral("TEXT"));
    addColumn(QStringLiteral("lockOwnerId"), QStringLiteral("TEXT"));
    addColumn(QStringLiteral("lockOwnerEditor"), QStringLiteral("TEXT"));
    addColumn(QStringLiteral("lockTime"), QStringLiteral("INTEGER"));
    addColumn(QStringLiteral("lockTimeout"), QStringLiteral("INTEGER"));
    addColumn(QStringLiteral("lockToken"), QStringLiteral("TEXT"));

    SqlQuery query(_db);
    query.prepare("CREATE INDEX IF NOT EXISTS caseconflicts_basePath ON caseconflicts(basePath);");
    if (!query.exec()) {
        sqlFail(QStringLiteral("caseconflictsTableStructure: create index basePath"), query);
        return re = false;
    }
    commitInternal(QStringLiteral("update database structure: add basePath index"));

    addColumn(QStringLiteral("isLivePhoto"), QStringLiteral("INTEGER"));
    addColumn(QStringLiteral("livePhotoFile"), QStringLiteral("TEXT"));

    {
        const auto quotaBytesUsed =  QStringLiteral("quotaBytesUsed");
        const auto quotaBytesAvailable =  QStringLiteral("quotaBytesAvailable");
        const auto defaultCommand = QStringLiteral("DEFAULT -1 NOT NULL");
        const auto bigInt = QStringLiteral("BIGINT");
        auto result = false;

        if (columnExists(quotaBytesUsed) && !hasDefaultValue(quotaBytesUsed)) {
            result = removeColumn(quotaBytesUsed);
        }

        if (columnExists(quotaBytesAvailable) && !hasDefaultValue(quotaBytesAvailable)) {
            result = removeColumn(quotaBytesAvailable);
        }

        if (result) {
            columns = tableColumns("metadata");
        }

        addColumn(quotaBytesUsed, bigInt, false, defaultCommand);
        addColumn(quotaBytesAvailable, bigInt, false, defaultCommand);
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
    QByteArray bytes = file;
#ifdef Q_OS_MACOS
    bytes = QString::fromUtf8(file).normalized(QString::NormalizationForm_C).toUtf8();
#endif

    qint64 h = 0;
    int len = bytes.length();

    h = c_jhash64((uint8_t *)bytes.data(), len, 0);
    return h;
}

Result<void, QString> SyncJournalDb::setFileRecord(const SyncJournalFileRecord &_record)
{
    SyncJournalFileRecord record = _record;
    QMutexLocker locker(&_mutex);

    Q_ASSERT(record._modtime > 0);
    if (record._modtime <= 0) {
        qCCritical(lcDb) << "invalid modification time";
    }

    if (!_etagStorageFilter.isEmpty()) {
        // If we are a directory that should not be read from db next time, don't write the etag
        QByteArray prefix = record._path + "/";
        for (const auto &it : std::as_const(_etagStorageFilter)) {
            if (it.startsWith(prefix)) {
                qCInfo(lcDb) << "Filtered writing the etag of" << prefix << "because it is a prefix of" << it;
                record._etag = "_invalid_";
                break;
            }
        }
    }

    qCInfo(lcDb) << "Updating file record for path:" << record.path() << "inode:" << record._inode
                 << "modtime:" << record._modtime << "type:" << record._type << "etag:" << record._etag
                 << "fileId:" << record._fileId << "remotePerm:" << record._remotePerm.toString()
                 << "fileSize:" << record._fileSize << "checksum:" << record._checksumHeader
                 << "e2eMangledName:" << record.e2eMangledName() << "isE2eEncrypted:" << record.isE2eEncrypted()
                 << "lock:" << (record._lockstate._locked ? "true" : "false")
                 << "lock owner type:" << record._lockstate._lockOwnerType
                 << "lock owner:" << record._lockstate._lockOwnerDisplayName
                 << "lock owner id:" << record._lockstate._lockOwnerId
                 << "lock editor:" << record._lockstate._lockEditorApp
                 << "sharedByMe:" << record._sharedByMe
                 << "isShared:" << record._isShared
                 << "lastShareStateFetchedTimestamp:" << record._lastShareStateFetchedTimestamp
                 << "isLivePhoto" << record._isLivePhoto
                 << "livePhotoFile" << record._livePhotoFile
                 << "folderQuota - bytesUsed:" << record._folderQuota.bytesUsed << "bytesAvailable:" << record._folderQuota.bytesAvailable;

    Q_ASSERT(!record.path().isEmpty());

    const qint64 phash = getPHash(record._path);
    if (!checkConnect()) {
        qCWarning(lcDb) << "Failed to connect database.";
        return tr("Failed to connect database."); // checkConnect failed.
    }

    int plen = record._path.length();

    QByteArray etag(record._etag);
    if (etag.isEmpty()) {
        etag = "";
    }
    QByteArray fileId(record._fileId);
    if (fileId.isEmpty()) {
        fileId = "";
    }
    QByteArray remotePerm = record._remotePerm.toDbValue();
    QByteArray checksumType, checksum;
    parseChecksumHeader(record._checksumHeader, &checksumType, &checksum);
    int contentChecksumTypeId = mapChecksumType(checksumType);

    const auto query = _queryManager.get(PreparedSqlQueryManager::SetFileRecordQuery, QByteArrayLiteral("INSERT OR REPLACE INTO metadata "
                                                                                                        "(phash, pathlen, path, inode, uid, gid, mode, modtime, type, md5, fileid, remotePerm, filesize, ignoredChildrenRemote, "
                                                                                                        "contentChecksum, contentChecksumTypeId, e2eMangledName, isE2eEncrypted, e2eCertificateFingerprint, lock, lockType, lockOwnerDisplayName, lockOwnerId, "
                                                                                                        "lockOwnerEditor, lockTime, lockTimeout, lockToken, isShared, lastShareStateFetchedTimestmap, sharedByMe, isLivePhoto, livePhotoFile, quotaBytesUsed, quotaBytesAvailable) "
                                                                                                        "VALUES (?1 , ?2, ?3 , ?4 , ?5 , ?6 , ?7,  ?8 , ?9 , ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23, ?24, ?25, ?26, ?27, ?28, ?29, ?30, ?31, ?32, ?33, ?34);"),
        _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
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
    query->bindValue(15, checksum);
    query->bindValue(16, contentChecksumTypeId);
    query->bindValue(17, record._e2eMangledName);
    query->bindValue(18, static_cast<int>(record._e2eEncryptionStatus));
    query->bindValue(19, {});
    query->bindValue(20, record._lockstate._locked ? 1 : 0);
    query->bindValue(21, record._lockstate._lockOwnerType);
    query->bindValue(22, record._lockstate._lockOwnerDisplayName);
    query->bindValue(23, record._lockstate._lockOwnerId);
    query->bindValue(24, record._lockstate._lockEditorApp);
    query->bindValue(25, record._lockstate._lockTime);
    query->bindValue(26, record._lockstate._lockTimeout);
    query->bindValue(27, record._lockstate._lockToken);
    query->bindValue(28, record._isShared);
    query->bindValue(29, record._lastShareStateFetchedTimestamp);
    query->bindValue(30, record._sharedByMe);
    query->bindValue(31, record._isLivePhoto);
    query->bindValue(32, record._livePhotoFile);
    query->bindValue(33, record._folderQuota.bytesUsed);
    query->bindValue(34, record._folderQuota.bytesAvailable);

    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return query->error();
    }

    // Can't be true anymore.
    _metadataTableIsEmpty = false;

    return {};
}

bool SyncJournalDb::getRootE2eFolderRecord(const QString &remoteFolderPath, SyncJournalFileRecord *rec)
{
    Q_ASSERT(rec);
    rec->_path.clear();
    Q_ASSERT(!rec->isValid());

    Q_ASSERT(!remoteFolderPath.isEmpty());

    Q_ASSERT(!remoteFolderPath.isEmpty() && remoteFolderPath != QStringLiteral("/"));
    if (remoteFolderPath.isEmpty() || remoteFolderPath == QStringLiteral("/")) {
        qCWarning(lcDb) << "Invalid folder path!";
        return false;
    }

    auto remoteFolderPathSplit = remoteFolderPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);

    if (remoteFolderPathSplit.isEmpty()) {
        qCWarning(lcDb) << "Invalid folder path!";
        return false;
    }

    while (!remoteFolderPathSplit.isEmpty()) {
        const auto result = getFileRecord(remoteFolderPathSplit.join(QLatin1Char('/')), rec);
        if (!result) {
            return false;
        }
        if (rec->isE2eEncrypted() && rec->_e2eMangledName.isEmpty()) {
            // it's a toplevel folder record
            return true;
        }
        remoteFolderPathSplit.removeLast();
    }

    return true;
}

bool SyncJournalDb::listAllE2eeFoldersWithEncryptionStatusLessThan(const int status, const std::function<void(const SyncJournalFileRecord &)> &rowCallback)
{
    QMutexLocker locker(&_mutex);

    if (_metadataTableIsEmpty)
        return true;

    if (!checkConnect())
        return false;
    const auto query = _queryManager.get(PreparedSqlQueryManager::ListAllTopLevelE2eeFoldersStatusLessThanQuery,
                                         QByteArrayLiteral(GET_FILE_RECORD_QUERY " WHERE type == 2 AND isE2eEncrypted >= ?1 AND isE2eEncrypted < ?2 ORDER BY path||'/' ASC"),
                                         _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }
    query->bindValue(1, SyncJournalFileRecord::EncryptionStatus::Encrypted);
    query->bindValue(2, status);

    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }

    forever {
        auto next = query->next();
        if (!next.ok) {
            qCWarning(lcDb) << "database error:" << query->error();
            return false;
        }

        if (!next.hasData) {
            break;
        }

        SyncJournalFileRecord rec;
        fillFileRecordFromGetQuery(rec, *query);

        if (rec._type == ItemTypeSkip) {
            continue;
        }

        rowCallback(rec);
    }

    return true;
}

bool SyncJournalDb::findEncryptedAncestorForRecord(const QString &filename, SyncJournalFileRecord *rec)
{
    Q_ASSERT(rec);
    rec->_path.clear();
    Q_ASSERT(!rec->isValid());

    const auto slashPosition = filename.lastIndexOf(QLatin1Char('/'));
    const auto parentPath = slashPosition >= 0 ? filename.left(slashPosition) : QString();

    auto pathComponents = parentPath.split(QLatin1Char('/'));
    while (!pathComponents.isEmpty()) {
        const auto pathCompontentsJointed = pathComponents.join(QLatin1Char('/'));
        if (!getFileRecord(pathCompontentsJointed, rec)) {
            qCWarning(lcDb) << "could not get file from local DB" << pathCompontentsJointed;
            return false;
        }

        if (rec->isValid() && rec->isE2eEncrypted()) {
            break;
        }
        pathComponents.removeLast();
    }
    return true;
}

void SyncJournalDb::keyValueStoreSet(const QString &key, QVariant value)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return;
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::SetKeyValueStoreQuery, QByteArrayLiteral("INSERT OR REPLACE INTO key_value_store (key, value) VALUES(?1, ?2);"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return;
    }

    query->bindValue(1, key);
    query->bindValue(2, value);

    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return;
    }
}

qint64 SyncJournalDb::keyValueStoreGetInt(const QString &key, qint64 defaultValue)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return defaultValue;
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::GetKeyValueStoreQuery, QByteArrayLiteral("SELECT value FROM key_value_store WHERE key=?1"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return defaultValue;
    }

    query->bindValue(1, key);
    query->exec();
    auto result = query->next();

    if (!result.ok || !result.hasData) {
        qCWarning(lcDb) << "database error:" << query->error();
        return defaultValue;
    }

    return query->int64Value(0);
}

QString SyncJournalDb::keyValueStoreGetString(const QString &key, const QString &defaultValue)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return defaultValue;
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::GetKeyValueStoreQuery, QByteArrayLiteral("SELECT value FROM key_value_store WHERE key=?1"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return defaultValue;
    }

    query->bindValue(1, key);
    query->exec();
    const auto result = query->next();

    if (!result.ok || !result.hasData) {
        qCWarning(lcDb) << "database error:" << query->error();
        return defaultValue;
    }

    return query->stringValue(0);
}

void SyncJournalDb::keyValueStoreDelete(const QString &key)
{
    const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteKeyValueStoreQuery, QByteArrayLiteral("DELETE FROM key_value_store WHERE key=?1;"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        qCWarning(lcDb) << "Failed to initOrReset _deleteKeyValueStoreQuery";
        Q_ASSERT(false);
    }
    query->bindValue(1, key);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        qCWarning(lcDb) << "Failed to exec _deleteKeyValueStoreQuery for key" << key;
        Q_ASSERT(false);
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
                qCWarning(lcDb) << "database error:" << query->error();
                return false;
            }

            const qint64 phash = getPHash(filename.toUtf8());
            query->bindValue(1, phash);

            if (!query->exec()) {
                qCWarning(lcDb) << "database error:" << query->error();
                return false;
            }
        }

        if (recursively) {
            const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteFileRecordRecursively, QByteArrayLiteral("DELETE FROM metadata WHERE " IS_PREFIX_PATH_OF("?1", "path")), _db);
            if (!query) {
                qCWarning(lcDb) << "database error:" << query->error();
                return false;
            }

            query->bindValue(1, filename);
            if (!query->exec()) {
                qCWarning(lcDb) << "database error:" << query->error();
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

    if (_metadataTableIsEmpty) {
        return true; // no error, yet nothing found (rec->isValid() == false)
    }

    if (!checkConnect()) {
        return false;
    }

    if (!filename.isEmpty()) {
        const auto query = _queryManager.get(PreparedSqlQueryManager::GetFileRecordQuery, QByteArrayLiteral(GET_FILE_RECORD_QUERY " WHERE phash=?1"), _db);
        if (!query) {
            qCWarning(lcDb) << "database error:" << query->error();
            return false;
        }

        query->bindValue(1, getPHash(filename));

        if (!query->exec()) {
            qCWarning(lcDb) << "database error:" << query->error();
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

bool SyncJournalDb::getFileRecordByE2eMangledName(const QString &mangledName, SyncJournalFileRecord *rec)
{
    QMutexLocker locker(&_mutex);

    // Reset the output var in case the caller is reusing it.
    Q_ASSERT(rec);
    rec->_path.clear();
    Q_ASSERT(!rec->isValid());

    if (_metadataTableIsEmpty) {
        return true; // no error, yet nothing found (rec->isValid() == false)
    }

    if (!checkConnect()) {
        return false;
    }

    if (!mangledName.isEmpty()) {
        const auto query = _queryManager.get(PreparedSqlQueryManager::GetFileRecordQueryByMangledName, QByteArrayLiteral(GET_FILE_RECORD_QUERY " WHERE e2eMangledName=?1"), _db);
        if (!query) {
            qCWarning(lcDb) << "database error:" << query->error();
            return false;
        }

        query->bindValue(1, mangledName);

        if (!query->exec()) {
            qCWarning(lcDb) << "database error:" << query->error();
            close();
            return false;
        }

        auto next = query->next();
        if (!next.ok) {
            QString err = query->error();
            qCWarning(lcDb) << "No journal entry found for mangled name" << mangledName << "Error: " << err;
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

    if (!inode || _metadataTableIsEmpty) {
        return true; // no error, yet nothing found (rec->isValid() == false)
    }

    if (!checkConnect()) {
        return false;
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::GetFileRecordQueryByInode, QByteArrayLiteral(GET_FILE_RECORD_QUERY " WHERE inode=?1"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }

    query->bindValue(1, inode);

    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }

    auto next = query->next();
    if (!next.ok) {
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }
    if (next.hasData) {
        fillFileRecordFromGetQuery(*rec, *query);
    }

    return true;
}

bool SyncJournalDb::getFileRecordsByFileId(const QByteArray &fileId, const std::function<void(const SyncJournalFileRecord &)> &rowCallback)
{
    QMutexLocker locker(&_mutex);

    if (fileId.isEmpty() || _metadataTableIsEmpty) {
        return true; // no error, yet nothing found (rec->isValid() == false)
    }

    if (!checkConnect()) {
        return false;
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::GetFileRecordQueryByFileId, QByteArrayLiteral(GET_FILE_RECORD_QUERY " WHERE fileid=?1"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }

    query->bindValue(1, fileId);

    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }

    forever {
        auto next = query->next();
        if (!next.ok) {
            qCWarning(lcDb) << "database error:" << query->error();
            return false;
        }

        if (!next.hasData) {
            break;
        }

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
            qCWarning(lcDb) << "database error:" << query.error();
            return false;
        }

        forever {
            auto next = query.next();
            if (!next.ok) {
                qCWarning(lcDb) << "database error:" << query.error();
                return false;
            }
            if (!next.hasData) {
                break;
            }

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

        const auto query = _queryManager.get(PreparedSqlQueryManager::GetAllFilesQuery, QByteArrayLiteral(GET_FILE_RECORD_QUERY " ORDER BY path||'/' ASC"), _db);
        if (!query) {
            qCWarning(lcDb) << "database error:" << query->error();
            return false;
        }
        return _exec(*query);
    } else {
        // This query is used to skip discovery and fill the tree from the
        // database instead
        const auto query = _queryManager.get(PreparedSqlQueryManager::GetFilesBelowPathQuery, QByteArrayLiteral(GET_FILE_RECORD_QUERY " WHERE " IS_PREFIX_PATH_OF("?1", "path")
                                                                                                                " OR " IS_PREFIX_PATH_OF("?1", "e2eMangledName")
                                                                                                                // We want to ensure that the contents of a directory are sorted
                                                                                                                // directly behind the directory itself. Without this ORDER BY
                                                                                                                // an ordering like foo, foo-2, foo/file would be returned.
                                                                                                                // With the trailing /, we get foo-2, foo, foo/file. This property
                                                                                                                // is used in fill_tree_from_db().
                                                                                                                " ORDER BY path||'/' ASC"),
            _db);
        if (!query) {
            qCWarning(lcDb) << "database error:" << query->error();
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

    if (_metadataTableIsEmpty) {
        return true;
    }

    if (!checkConnect()) {
        return false;
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::ListFilesInPathQuery, QByteArrayLiteral(GET_FILE_RECORD_QUERY " WHERE parent_hash(path) = ?1 ORDER BY path||'/' ASC"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }
    query->bindValue(1, getPHash(path));

    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }

    forever {
        auto next = query->next();
        if (!next.ok) {
            qCWarning(lcDb) << "database error:" << query->error();
            return false;
        }

        if (!next.hasData) {
            break;
        }

        SyncJournalFileRecord rec;
        fillFileRecordFromGetQuery(rec, *query);
        if (!rec._path.startsWith(path) || rec._path.indexOf("/", path.size() + 1) > 0) {
            qWarning(lcDb) << "hash collision" << path << rec.path();
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

bool SyncJournalDb::ensureCorrectEncryptionStatus()
{
    qCInfo(lcDb) << "migration: ensure proper encryption status in database";

    const auto folderQuery = _queryManager.get(PreparedSqlQueryManager::FolderUpdateInvalidEncryptionStatus, QByteArrayLiteral("UPDATE "
                                                                                                                         "metadata AS invalidItem "
                                                                                                                         "SET "
                                                                                                                         "isE2eEncrypted = 4 "
                                                                                                                         "WHERE "
                                                                                                                         "invalidItem.isE2eEncrypted = 0 AND "
                                                                                                                         "(invalidItem.type = 0 OR invalidItem.type = 2) AND "
                                                                                                                         "EXISTS ( "
                                                                                                                         "SELECT * "
                                                                                                                         "FROM "
                                                                                                                         "metadata parentFolder "
                                                                                                                         "WHERE "
                                                                                                                         "parent_hash(invalidItem.path) = parentFolder.phash AND "
                                                                                                                         "parentFolder.isE2eEncrypted <> 0)"),
                                         _db);
    if (!folderQuery) {
        qCWarning(lcDb) << "database error:" << folderQuery->error();
        return false;
    }

    if (!folderQuery->exec()) {
        qCWarning(lcDb) << "database error:" << folderQuery->error();
        return false;
    }

    return true;
}

bool SyncJournalDb::updateFileRecordChecksum(const QString &filename,
    const QByteArray &contentChecksum,
    const QByteArray &contentChecksumType)
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
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }
    query->bindValue(1, phash);
    query->bindValue(2, contentChecksum);
    query->bindValue(3, checksumTypeId);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }

    return true;
}

bool SyncJournalDb::updateLocalMetadata(const QString &filename,
    qint64 modtime, qint64 size, quint64 inode, const SyncJournalFileLockInfo &lockInfo)

{
    QMutexLocker locker(&_mutex);

    qCInfo(lcDb) << "Updating local metadata for:" << filename << modtime << size << inode;

    const qint64 phash = getPHash(filename.toUtf8());
    if (!checkConnect()) {
        qCWarning(lcDb) << "Failed to connect database.";
        return false;
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::SetFileRecordLocalMetadataQuery, QByteArrayLiteral("UPDATE metadata"
                                                                                                                     " SET inode=?2, modtime=?3, filesize=?4, lock=?5, lockType=?6,"
                                                                                                                     " lockOwnerDisplayName=?7, lockOwnerId=?8, lockOwnerEditor = ?9,"
                                                                                                                     " lockTime=?10, lockTimeout=?11, lockToken=?12"
                                                                                                                     " WHERE phash == ?1;"),
        _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }

    query->bindValue(1, phash);
    query->bindValue(2, inode);
    query->bindValue(3, modtime);
    query->bindValue(4, size);
    query->bindValue(5, lockInfo._locked ? 1 : 0);
    query->bindValue(6, lockInfo._lockOwnerType);
    query->bindValue(7, lockInfo._lockOwnerDisplayName);
    query->bindValue(8, lockInfo._lockOwnerId);
    query->bindValue(9, lockInfo._lockEditorApp);
    query->bindValue(10, lockInfo._lockTime);
    query->bindValue(11, lockInfo._lockTimeout);
    query->bindValue(12, lockInfo._lockToken);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }
    return true;
}

bool SyncJournalDb::hasFileIds(const QList<qint64> &fileIds)
{
    if (fileIds.isEmpty()) {
        // no need to check the db if no file id matches
        return false;
    }

    QMutexLocker locker(&_mutex);

    if (!checkConnect()) {
        return false;
    }

    // quick workaround for looking up pure numeric file IDs: cast it to integer
    //
    // using `IN` with a list of IDs does not allow for a prepared query: the execution plan
    // would be different depending on the amount of elements as each element is added to a
    // temporary table one-by-one.  with `json_each()`, all that changes is one string
    // parameter which is perfect for creating a prepared query :)
    const auto query = _queryManager.get(
        PreparedSqlQueryManager::HasFileIdQuery,
        "SELECT 1 FROM metadata, json_each(?1) file_ids WHERE CAST(metadata.fileid AS INTEGER) = CAST(file_ids.value AS INTEGER) LIMIT 1;"_ba,
        _db
    );
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return false;
    }

    // we have a prepared query, so let's build up a JSON array of file IDs to check for.
    // Strings are used here to avoid any surprises during serialisation: JSON only really
    // has a double type, and who knows when the representation changes to the +e* variant.
    QJsonArray fileIdStrings = {};
    for (const auto &fileId : fileIds) {
        fileIdStrings.append(QString::number(fileId));
    }
    const auto fileIdsParameter = QJsonDocument(fileIdStrings).toJson(QJsonDocument::Compact);
    query->bindValue(1, fileIdsParameter);

    if (!query->exec()) {
        qCWarning(lcDb) << "file id query failed:" << query->error();
        return false;
    }

    if (query->next().hasData && query->intValue(0) == 1) {
        // at least one file ID from the passed list is present
        return true;
    }

    return false;
}

Optional<SyncJournalDb::HasHydratedDehydrated> SyncJournalDb::hasHydratedOrDehydratedFiles(const QByteArray &filename)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return {};
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::CountDehydratedFilesQuery, QByteArrayLiteral("SELECT DISTINCT type FROM metadata"
                                                                                                               " WHERE (" IS_PREFIX_PATH_OR_EQUAL("?1", "path") " OR ?1 == '');"),
        _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return {};
    }

    query->bindValue(1, filename);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return {};
    }

    HasHydratedDehydrated result;
    forever {
        auto next = query->next();
        if (!next.ok) {
            qCWarning(lcDb) << "database error:" << query->error();
            return {};
        }
        if (!next.hasData) {
            break;
        }
        auto type = static_cast<ItemType>(query->intValue(0));
        if (type == ItemTypeFile || type == ItemTypeVirtualFileDehydration) {
            result.hasHydrated = true;
        }
        if (type == ItemTypeVirtualFile || type == ItemTypeVirtualFileDownload) {
            result.hasDehydrated = true;
        }
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

static bool deleteBatch(SqlQuery &query, const QStringList &entries)
{
    if (entries.isEmpty())
        return true;

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
            qCWarning(lcDb) << "database error:" << query->error();
            return res;
        }

        query->bindValue(1, file);

        if (!query->exec()) {
            qCWarning(lcDb) << "database error:" << query->error();
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
            qCWarning(lcDb) << "database error:" << query->error();
            return;
        }
        query->bindValue(1, file);
        query->bindValue(2, i._tmpfile);
        query->bindValue(3, i._etag);
        query->bindValue(4, i._errorCount);
        if (!query->exec()) {
            qCWarning(lcDb) << "database error:" << query->error();
        }
    } else {
        const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteDownloadInfoQuery);
        if (!query) {
            qCWarning(lcDb) << "database error:" << query->error();
            return;
        }
        query->bindValue(1, file);
        if (!query->exec()) {
            qCWarning(lcDb) << "database error:" << query->error();
        }
    }
}

QVector<SyncJournalDb::DownloadInfo> SyncJournalDb::getAndDeleteStaleDownloadInfos(const QSet<QString> &keep)
{
    QVector<SyncJournalDb::DownloadInfo> empty_result;
    QMutexLocker locker(&_mutex);

    if (!checkConnect()) {
        return empty_result;
    }

    SqlQuery query(_db);
    // The selected values *must* match the ones expected by toDownloadInfo().
    query.prepare("SELECT tmpfile, etag, errorcount, path FROM downloadinfo");

    if (!query.exec()) {
        qCWarning(lcDb) << "database error:" << query.error();
        return empty_result;
    }

    QStringList superfluousPaths;
    QVector<SyncJournalDb::DownloadInfo> deleted_entries;

    while (query.next().hasData) {
        const QString file = query.stringValue(3); // path
        if (!keep.contains(file)) {
            superfluousPaths.append(file);
            DownloadInfo info;
            toDownloadInfo(query, &info);
            deleted_entries.append(info);
        }
    }

    {
        const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteDownloadInfoQuery);
        if (!query) {
            qCWarning(lcDb) << "database error:" << query->error();
            return empty_result;
        }
        if (!deleteBatch(*query, superfluousPaths)) {
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

    UploadInfo res;

    if (checkConnect()) {
        const auto query = _queryManager.get(PreparedSqlQueryManager::GetUploadInfoQuery, QByteArrayLiteral("SELECT chunk, transferid, errorcount, size, modtime, contentChecksum FROM "
                                                                                                            "uploadinfo WHERE path=?1"),
            _db);
        if (!query) {
            qCWarning(lcDb) << "database error:" << query->error();
            return res;
        }
        query->bindValue(1, file);

        if (!query->exec()) {
            qCWarning(lcDb) << "database error:" << query->error();
            return res;
        }

        if (query->next().hasData) {
            bool ok = true;
            res._chunkUploadV1 = query->intValue(0);
            res._transferid = query->int64Value(1);
            res._errorCount = query->intValue(2);
            res._size = query->int64Value(3);
            res._modtime = query->int64Value(4);
            res._contentChecksum = query->baValue(5);
            res._valid = ok;
        }
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
                                                                                                            "(path, chunk, transferid, errorcount, size, modtime, contentChecksum) "
                                                                                                            "VALUES ( ?1 , ?2, ?3 , ?4 ,  ?5, ?6 , ?7 )"),
            _db);
        if (!query) {
            qCWarning(lcDb) << "database error:" << query->error();
            return;
        }

        query->bindValue(1, file);
        query->bindValue(2, i._chunkUploadV1);
        query->bindValue(3, i._transferid);
        query->bindValue(4, i._errorCount);
        query->bindValue(5, i._size);
        query->bindValue(6, i._modtime);
        query->bindValue(7, i._contentChecksum);

        if (!query->exec()) {
            qCWarning(lcDb) << "database error:" << query->error();
            return;
        }
    } else {
        const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteUploadInfoQuery);
        if (!query) {
            qCWarning(lcDb) << "database error:" << query->error();
            return;
        }

        query->bindValue(1, file);

        if (!query->exec()) {
            qCWarning(lcDb) << "database error:" << query->error();
            return;
        }
    }
}

QVector<uint> SyncJournalDb::deleteStaleUploadInfos(const QSet<QString> &keep)
{
    QMutexLocker locker(&_mutex);
    QVector<uint> ids;

    if (!checkConnect()) {
        return ids;
    }

    SqlQuery query(_db);
    query.prepare("SELECT path,transferid FROM uploadinfo");

    if (!query.exec()) {
        return ids;
    }

    QStringList superfluousPaths;

    while (query.next().hasData) {
        const QString file = query.stringValue(0);
        if (!keep.contains(file)) {
            superfluousPaths.append(file);
            ids.append(query.intValue(1));
        }
    }

    const auto deleteUploadInfoQuery = _queryManager.get(PreparedSqlQueryManager::DeleteUploadInfoQuery);
    deleteBatch(*deleteUploadInfoQuery, superfluousPaths);
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
        if (!query) {
            qCWarning(lcDb) << "database error:" << query->error();
            return entry;
        }

        query->bindValue(1, file);
        if (!query->exec()) {
            qCWarning(lcDb) << "database error:" << query->error();
            return entry;
        }

        if (query->next().hasData) {
            entry._lastTryEtag = query->baValue(0);
            entry._lastTryModtime = query->int64Value(1);
            entry._retryCount = query->intValue(2);
            entry._errorString = query->stringValue(3);
            entry._lastTryTime = query->int64Value(4);
            entry._ignoreDuration = query->int64Value(5);
            entry._renameTarget = query->stringValue(6);
            entry._errorCategory = static_cast<SyncJournalErrorBlacklistRecord::Category>(query->intValue(7));
            entry._requestId = query->baValue(8);
            entry._file = file;
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
    return deleteBatch(delQuery, superfluousPaths);
}

void SyncJournalDb::deleteStaleFlagsEntries()
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect())
        return;

    SqlQuery delQuery("DELETE FROM flags WHERE path != '' AND path NOT IN (SELECT path from metadata);", _db);
    if (!delQuery.exec()) {
        sqlFail(QStringLiteral("deleteStaleFlagsEntries"), delQuery);
    }
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

void SyncJournalDb::wipeErrorBlacklistEntry(const QString &file)
{
    if (file.isEmpty()) {
        return;
    }

    QMutexLocker locker(&_mutex);
    if (checkConnect()) {
        SqlQuery query(_db);

        query.prepare("DELETE FROM blacklist WHERE path=?1");
        query.bindValue(1, file);
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
        qCWarning(lcDb) << "database error:" << query->error();
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
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return;
    }
}

QVector<SyncJournalDb::PollInfo> SyncJournalDb::getPollInfos()
{
    QMutexLocker locker(&_mutex);

    QVector<SyncJournalDb::PollInfo> res;

    if (!checkConnect())
        return res;

    SqlQuery query("SELECT path, modtime, filesize, pollpath FROM async_poll", _db);

    if (!query.exec()) {
        return res;
    }

    while (query.next().hasData) {
        PollInfo info;
        info._file = query.stringValue(0);
        info._modtime = query.int64Value(1);
        info._fileSize = query.int64Value(2);
        info._url = query.stringValue(3);
        res.append(info);
    }
    return res;
}

void SyncJournalDb::setPollInfo(const SyncJournalDb::PollInfo &info)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return;
    }

    if (info._url.isEmpty()) {
        qCDebug(lcDb) << "Deleting Poll job" << info._file;
        SqlQuery query("DELETE FROM async_poll WHERE path=?", _db);
        query.bindValue(1, info._file);
        if (!query.exec()) {
            sqlFail(QStringLiteral("setPollInfo DELETE FROM async_poll"), query);
        }
    } else {
        SqlQuery query("INSERT OR REPLACE INTO async_poll (path, modtime, filesize, pollpath) VALUES( ? , ? , ? , ? )", _db);
        query.bindValue(1, info._file);
        query.bindValue(2, info._modtime);
        query.bindValue(3, info._fileSize);
        query.bindValue(4, info._url);
        if (!query.exec()) {
            sqlFail(QStringLiteral("setPollInfo INSERT OR REPLACE INTO async_poll"), query);
        }
    }
}

QStringList SyncJournalDb::getSelectiveSyncList(SyncJournalDb::SelectiveSyncListType type, bool *ok)
{
    QStringList result;
    ASSERT(ok);

    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        *ok = false;
        return result;
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::GetSelectiveSyncListQuery, QByteArrayLiteral("SELECT path FROM selectivesync WHERE type=?1"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        *ok = false;
        return result;
    }

    query->bindValue(1, int(type));
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        *ok = false;
        return result;
    }
    forever {
        auto next = query->next();
        if (!next.ok) {
            qCWarning(lcDb) << "database error:" << query->error();
            *ok = false;
            return result;
        }
        if (!next.hasData)
            break;

        const auto entry = Utility::trailingSlashPath(query->stringValue(0));
        result.append(entry);
    }
    *ok = true;

    return result;
}

void SyncJournalDb::setSelectiveSyncList(SyncJournalDb::SelectiveSyncListType type, const QStringList &list)
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

QStringList SyncJournalDb::addSelectiveSyncLists(SelectiveSyncListType type, const QString &path)
{
    bool ok = false;

    const auto pathWithTrailingSlash = Utility::trailingSlashPath(path);

    const auto blackListList = getSelectiveSyncList(type, &ok);
    auto blackListSet = QSet<QString>{blackListList.begin(), blackListList.end()};
    blackListSet.insert(pathWithTrailingSlash);
    auto blackList = blackListSet.values();
    blackList.sort();
    setSelectiveSyncList(type, blackList);

    qCInfo(lcSql()) << "add" << path << "into" << type << blackList;

    return blackList;
}

QStringList SyncJournalDb::removeSelectiveSyncLists(SelectiveSyncListType type, const QString &path)
{
    bool ok = false;

    const auto pathWithTrailingSlash = Utility::trailingSlashPath(path);

    const auto blackListList = getSelectiveSyncList(type, &ok);
    auto blackListSet = QSet<QString>{blackListList.begin(), blackListList.end()};
    blackListSet.remove(pathWithTrailingSlash);
    auto blackList = blackListSet.values();
    blackList.sort();
    setSelectiveSyncList(type, blackList);

    qCInfo(lcSql()) << "remove" << path << "into" << type << blackList;

    return blackList;
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

    if (!query.exec()) {
        sqlFail(QStringLiteral("avoidRenamesOnNextSync path: %1").arg(QString::fromUtf8(path)), query);
    }

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
    // Note: CSYNC_FTW_TYPE_DIR == 2
    query.prepare("UPDATE metadata SET md5='_invalid_' WHERE " IS_PREFIX_PATH_OR_EQUAL("path", "?1") " AND type == 2;");
    query.bindValue(1, argument);

    if (!query.exec()) {
        sqlFail(QStringLiteral("schedulePathForRemoteDiscovery path: %1").arg(QString::fromUtf8(fileName)), query);
    }

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

    if (!deleteRemoteFolderEtagsQuery.exec()) {
        sqlFail(QStringLiteral("forceRemoteDiscoveryNextSyncLocked"), deleteRemoteFolderEtagsQuery);
    }
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
        qCWarning(lcDb) << "database error:" << query->error();
        return {};
    }
    query->bindValue(1, checksumTypeId);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return QByteArray();
    }

    if (!query->next().hasData) {
        qCWarning(lcDb) << "No checksum type mapping found for" << checksumTypeId;
        return QByteArray();
    }
    return query->baValue(0);
}

int SyncJournalDb::mapChecksumType(const QByteArray &checksumType)
{
    if (checksumType.isEmpty()) {
        return 0;
    }

    auto it =  _checksymTypeCache.find(checksumType);
    if (it != _checksymTypeCache.end())
        return *it;

    // Ensure the checksum type is in the db
    {
        const auto query = _queryManager.get(PreparedSqlQueryManager::InsertChecksumTypeQuery, QByteArrayLiteral("INSERT OR IGNORE INTO checksumtype (name) VALUES (?1)"), _db);
        if (!query) {
            qCWarning(lcDb) << "database error:" << query->error();
            return 0;
        }
        query->bindValue(1, checksumType);
        if (!query->exec()) {
            qCWarning(lcDb) << "database error:" << query->error();
            return 0;
        }
    }

    // Retrieve the id
    {
        const auto query = _queryManager.get(PreparedSqlQueryManager::GetChecksumTypeIdQuery, QByteArrayLiteral("SELECT id FROM checksumtype WHERE name=?1"), _db);
        if (!query) {
            qCWarning(lcDb) << "database error:" << query->error();
            return 0;
        }
        query->bindValue(1, checksumType);
        if (!query->exec()) {
            qCWarning(lcDb) << "database error:" << query->error();
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
        qCWarning(lcDb) << "database error:" << query->error();
        return QByteArray();
    }

    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
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
    if (!setDataFingerprintQuery1) {
        qCWarning(lcDb) << "database error:" << setDataFingerprintQuery1->error();
        return;
    }
    if (!setDataFingerprintQuery2) {
        qCWarning(lcDb) << "database error:" << setDataFingerprintQuery2->error();
        return;
    }

    if (!setDataFingerprintQuery1->exec()) {
        qCWarning(lcDb) << "database error:" << setDataFingerprintQuery1->error();
    }

    setDataFingerprintQuery2->bindValue(1, dataFingerprint);
    if (!setDataFingerprintQuery2->exec()) {
        qCWarning(lcDb) << "database error:" << setDataFingerprintQuery2->error();
    }
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

    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return;
    }

    query->bindValue(1, record.path);
    query->bindValue(2, record.baseFileId);
    query->bindValue(3, record.baseModtime);
    query->bindValue(4, record.baseEtag);
    query->bindValue(5, record.initialBasePath);
    if(!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
    }
}

ConflictRecord SyncJournalDb::conflictRecord(const QByteArray &path)
{
    ConflictRecord entry;

    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return entry;
    }
    const auto query = _queryManager.get(PreparedSqlQueryManager::GetConflictRecordQuery, QByteArrayLiteral("SELECT baseFileId, baseModtime, baseEtag, basePath FROM conflicts WHERE path=?1;"), _db);
    if(!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return entry;
    }

    query->bindValue(1, path);
    if(!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return entry;
    }
    if (!query->next().hasData)
        return entry;

    entry.path = path;
    entry.baseFileId = query->baValue(0);
    entry.baseModtime = query->int64Value(1);
    entry.baseEtag = query->baValue(2);
    entry.initialBasePath = query->baValue(3);
    return entry;
}

void SyncJournalDb::setCaseConflictRecord(const ConflictRecord &record)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect())
        return;

    const auto query = _queryManager.get(PreparedSqlQueryManager::SetCaseClashConflictRecordQuery, QByteArrayLiteral("INSERT OR REPLACE INTO caseconflicts "
                                                                                                            "(path, baseFileId, baseModtime, baseEtag, basePath) "
                                                                                                            "VALUES (?1, ?2, ?3, ?4, ?5);"),
                                         _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return;
    }

    query->bindValue(1, record.path);
    query->bindValue(2, record.baseFileId);
    query->bindValue(3, record.baseModtime);
    query->bindValue(4, record.baseEtag);
    query->bindValue(5, record.initialBasePath);
    if(!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
    }
}

ConflictRecord SyncJournalDb::caseConflictRecordByBasePath(const QString &baseNamePath)
{
    ConflictRecord entry;

    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return entry;
    }
    const auto query = _queryManager.get(PreparedSqlQueryManager::GetCaseClashConflictRecordQuery, QByteArrayLiteral("SELECT path, baseFileId, baseModtime, baseEtag, basePath FROM caseconflicts WHERE basePath=?1;"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return entry;
    }
    query->bindValue(1, baseNamePath);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return entry;
    }
    if (!query->next().hasData)
        return entry;

    entry.path = query->baValue(0);
    entry.baseFileId = query->baValue(1);
    entry.baseModtime = query->int64Value(2);
    entry.baseEtag = query->baValue(3);
    entry.initialBasePath = query->baValue(4);
    return entry;
}

ConflictRecord SyncJournalDb::caseConflictRecordByPath(const QString &path)
{
    ConflictRecord entry;

    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return entry;
    }
    const auto query = _queryManager.get(PreparedSqlQueryManager::GetCaseClashConflictRecordByPathQuery, QByteArrayLiteral("SELECT path, baseFileId, baseModtime, baseEtag, basePath FROM caseconflicts WHERE path=?1;"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return entry;
    }
    query->bindValue(1, path);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return entry;
    }
    if (!query->next().hasData)
        return entry;

    entry.path = query->baValue(0);
    entry.baseFileId = query->baValue(1);
    entry.baseModtime = query->int64Value(2);
    entry.baseEtag = query->baValue(3);
    entry.initialBasePath = query->baValue(4);
    return entry;
}

void SyncJournalDb::deleteCaseClashConflictByPathRecord(const QString &path)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect())
        return;

    const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteCaseClashConflictRecordQuery, QByteArrayLiteral("DELETE FROM caseconflicts WHERE path=?1;"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return;
    }
    query->bindValue(1, path);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
    }
}

QByteArrayList SyncJournalDb::caseClashConflictRecordPaths()
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return {};
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::GetAllCaseClashConflictPathQuery, QByteArrayLiteral("SELECT path FROM caseconflicts;"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return {};
    }
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return {};
    }

    QByteArrayList paths;
    while (query->next().hasData)
        paths.append(query->baValue(0));

    return paths;
}

void SyncJournalDb::deleteConflictRecord(const QByteArray &path)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect())
        return;

    const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteConflictRecordQuery, QByteArrayLiteral("DELETE FROM conflicts WHERE path=?1;"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return;
    }
    query->bindValue(1, path);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
    }
}

QByteArrayList SyncJournalDb::conflictRecordPaths()
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect())
        return {};

    SqlQuery query(_db);
    query.prepare("SELECT path FROM conflicts");
    if (!query.exec()) {
        qCWarning(lcDb) << "database error:" << query.error();
        return {};
    }

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
        if (!getFileRecordsByFileId(conflict.baseFileId, [&result](const SyncJournalFileRecord &record) {
            if (!record._path.isEmpty())
                result = record._path;
        })) {
            qCWarning(lcDb) << "conflictFileBaseName failed to getFileRecordsByFileId: " << conflictName;
        }
    }

    if (result.isEmpty()) {
        result = Utility::conflictFileBaseNameFromPattern(conflictName);
    }
    return result;
}

void SyncJournalDb::clearFileTable()
{
    QMutexLocker lock(&_mutex);
    SqlQuery query(_db);
    query.prepare("DELETE FROM metadata;");

    if (!query.exec()) {
        qCWarning(lcDb) << "database error:" << query.error();
        sqlFail(QStringLiteral("clearFileTable"), query);
    }
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

    if (!query.exec()) {
        qCWarning(lcDb) << "database error:" << query.error();
        sqlFail(QStringLiteral("markVirtualFileForDownloadRecursively UPDATE metadata SET type=5 path: %1").arg(QString::fromUtf8(path)), query);
    }

    // We also must make sure we do not read the files from the database (same logic as in schedulePathForRemoteDiscovery)
    // This includes all the parents up to the root, but also all the directory within the selected dir.
    static_assert(ItemTypeDirectory == 2, "");
    query.prepare("UPDATE metadata SET md5='_invalid_' WHERE "
                  "(" IS_PREFIX_PATH_OF("?1", "path") " OR ?1 == '' OR " IS_PREFIX_PATH_OR_EQUAL("path", "?1") ") AND type == 2;");
    query.bindValue(1, path);

    if (!query.exec()) {
        qCWarning(lcDb) << "database error:" << query.error();
        sqlFail(QStringLiteral("markVirtualFileForDownloadRecursively UPDATE metadata SET md5='_invalid_' path: %1").arg(QString::fromUtf8(path)), query);
    }
}

void SyncJournalDb::setE2EeLockedFolder(const QByteArray &folderId, const QByteArray &folderToken)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return;
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::SetE2EeLockedFolderQuery,
                                         QByteArrayLiteral("INSERT OR REPLACE INTO e2EeLockedFolders "
                                                           "(folderId, token) "
                                                           "VALUES (?1, ?2);"),
                                         _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return;
    }
    query->bindValue(1, folderId);
    query->bindValue(2, folderToken);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
    }
}

QByteArray SyncJournalDb::e2EeLockedFolder(const QByteArray &folderId)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return {};
    }
    const auto query = _queryManager.get(PreparedSqlQueryManager::GetE2EeLockedFolderQuery,
                                         QByteArrayLiteral("SELECT token FROM e2EeLockedFolders WHERE folderId=?1;"),
                                         _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return {};
    }
    query->bindValue(1, folderId);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return {};
    }
    if (!query->next().hasData) {
        return {};
    }

    return query->baValue(0);
}

QList<QPair<QByteArray, QByteArray>> SyncJournalDb::e2EeLockedFolders()
{
    QMutexLocker locker(&_mutex);

    QList<QPair<QByteArray, QByteArray>> res;

    if (!checkConnect()) {
        return res;
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::GetE2EeLockedFoldersQuery, QByteArrayLiteral("SELECT * FROM e2EeLockedFolders"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return res;
    }

    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return res;
    }

    while (query->next().hasData) {
        res.append({query->baValue(0), query->baValue(1)});
    }
    return res;
}

void SyncJournalDb::deleteE2EeLockedFolder(const QByteArray &folderId)
{
    QMutexLocker locker(&_mutex);
    if (!checkConnect()) {
        return;
    }

    const auto query = _queryManager.get(PreparedSqlQueryManager::DeleteE2EeLockedFolderQuery, QByteArrayLiteral("DELETE FROM e2EeLockedFolders WHERE folderId=?1;"), _db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return;
    }
    query->bindValue(1, folderId);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
    }
}

Optional<PinState> SyncJournalDb::PinStateInterface::rawForPath(const QByteArray &path)
{
    QMutexLocker lock(&_db->_mutex);
    if (!_db->checkConnect())
        return {};

    const auto query = _db->_queryManager.get(PreparedSqlQueryManager::GetRawPinStateQuery, QByteArrayLiteral("SELECT pinState FROM flags WHERE path == ?1;"), _db->_db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return {};
    }
    query->bindValue(1, path);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return {};
    }

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
    if (!_db->checkConnect()) {
        return {};
    }

    const auto query = _db->_queryManager.get(PreparedSqlQueryManager::GetEffectivePinStateQuery, QByteArrayLiteral("SELECT pinState FROM flags WHERE"
                                                                                                                    // explicitly allow "" to represent the root path
                                                                                                                    // (it'd be great if paths started with a / and "/" could be the root)
                                                                                                                    " (" IS_PREFIX_PATH_OR_EQUAL("path", "?1") " OR path == '')"
                                                                                                                                                               " AND pinState is not null AND pinState != 0"
                                                                                                                                                               " ORDER BY length(path) DESC LIMIT 1;"),
        _db->_db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return {};
    }
    query->bindValue(1, path);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return {};
    }

    auto next = query->next();
    if (!next.ok)
        return {};
    // If the root path has no setting, assume AlwaysLocal
    if (!next.hasData)
        return PinState::AlwaysLocal;

    return static_cast<PinState>(query->intValue(0));
}

Optional<PinState> SyncJournalDb::PinStateInterface::effectiveForPathRecursive(const QByteArray &path)
{
    // Get the item's effective pin state. We'll compare subitem's pin states
    // against this.
    const auto basePin = effectiveForPath(path);
    if (!basePin) {
        return {};
    }

    QMutexLocker lock(&_db->_mutex);
    if (!_db->checkConnect()) {
        return {};
    }

    // Find all the non-inherited pin states below the item
    const auto query = _db->_queryManager.get(PreparedSqlQueryManager::GetSubPinsQuery, QByteArrayLiteral("SELECT DISTINCT pinState FROM flags WHERE"
                                                                                                          " (" IS_PREFIX_PATH_OF("?1", "path") " OR ?1 == '')"
                                                                                                                                               " AND pinState is not null and pinState != 0;"),
        _db->_db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return {};
    }
    query->bindValue(1, path);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
        return {};
    }

    // Check if they are all identical
    forever {
        auto next = query->next();
        if (!next.ok) {
            qCWarning(lcDb) << "database error:" << query->error();
            return {};
        }
        if (!next.hasData) {
            break;
        }
        const auto subPin = static_cast<PinState>(query->intValue(0));
        if (subPin != *basePin) {
            return PinState::Inherited;
        }
    }

    return *basePin;
}

void SyncJournalDb::PinStateInterface::setForPath(const QByteArray &path, PinState state)
{
    QMutexLocker lock(&_db->_mutex);
    if (!_db->checkConnect()) {
        return;
    }

    const auto query = _db->_queryManager.get(PreparedSqlQueryManager::SetPinStateQuery, QByteArrayLiteral(
                                                                                             // If we had sqlite >=3.24.0 everywhere this could be an upsert,
                                                                                             // making further flags columns easy
                                                                                             //"INSERT INTO flags(path, pinState) VALUES(?1, ?2)"
                                                                                             //" ON CONFLICT(path) DO UPDATE SET pinState=?2;"),
                                                                                             // Simple version that doesn't work nicely with multiple columns:
                                                                                             "INSERT OR REPLACE INTO flags(path, pinState) VALUES(?1, ?2);"),
        _db->_db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return;
    }
    query->bindValue(1, path);
    query->bindValue(2, state);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
    }
}

void SyncJournalDb::PinStateInterface::wipeForPathAndBelow(const QByteArray &path)
{
    QMutexLocker lock(&_db->_mutex);
    if (!_db->checkConnect()) {
        return;
    }

    const auto query = _db->_queryManager.get(PreparedSqlQueryManager::WipePinStateQuery, QByteArrayLiteral("DELETE FROM flags WHERE "
                                                                                                            // Allow "" to delete everything
                                                                                                            " (" IS_PREFIX_PATH_OR_EQUAL("?1", "path") " OR ?1 == '');"),
        _db->_db);
    if (!query) {
        qCWarning(lcDb) << "database error:" << query->error();
        return;
    }
    query->bindValue(1, path);
    if (!query->exec()) {
        qCWarning(lcDb) << "database error:" << query->error();
    }
}

Optional<QVector<QPair<QByteArray, PinState>>>
SyncJournalDb::PinStateInterface::rawList()
{
    QMutexLocker lock(&_db->_mutex);
    if (!_db->checkConnect()) {
        return {};
    }

    SqlQuery query("SELECT path, pinState FROM flags;", _db->_db);

    if (!query.exec()) {
        qCWarning(lcDb) << "SQL Error" << "PinStateInterface::rawList" << query.error();
        _db->close();
        ASSERT(false);
    }

    QVector<QPair<QByteArray, PinState>> result;
    forever {
        auto next = query.next();
        if (!next.ok) {
            qCWarning(lcDb) << "database error:" << query.error();
            return {};
        }
        if (!next.hasData) {
            break;
        }
        result.append({ query.baValue(0), static_cast<PinState>(query.intValue(1)) });
    }
    return result;
}

SyncJournalDb::PinStateInterface SyncJournalDb::internalPinStates()
{
    return PinStateInterface{this};
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

bool SyncJournalDb::isOpen()
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
    if (isOpen()) {
        close();
    }
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
    return lhs._errorCount == rhs._errorCount && lhs._chunkUploadV1 == rhs._chunkUploadV1 && lhs._modtime == rhs._modtime && lhs._valid == rhs._valid
        && lhs._size == rhs._size && lhs._transferid == rhs._transferid && lhs._contentChecksum == rhs._contentChecksum;
}

QDebug& operator<<(QDebug &stream, const SyncJournalFileRecord::EncryptionStatus status)
{
    stream << static_cast<int>(status);
    return stream;
}

} // namespace OCC
