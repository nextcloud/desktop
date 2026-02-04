/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SYNCJOURNALDB_H
#define SYNCJOURNALDB_H

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QVariant>
#include <functional>

#include "common/utility.h"
#include "common/ownsql.h"
#include "common/preparedsqlquerymanager.h"
#include "common/syncjournalfilerecord.h"
#include "common/result.h"
#include "common/pinstate.h"

class TestSyncJournalDB;

namespace OCC {
class SyncJournalFileRecord;

/**
 * @brief Class that handles the sync database
 *
 * This class is thread safe. All public functions lock the mutex.
 * @ingroup libsync
 */
class OCSYNC_EXPORT SyncJournalDb : public QObject
{
    Q_OBJECT
public:
    explicit SyncJournalDb(const QString &dbFilePath, QObject *parent = nullptr);
    ~SyncJournalDb() override;

    /// Create a journal path for a specific configuration
    static QString makeDbName(const QString &localPath,
        const QUrl &remoteUrl,
        const QString &remotePath,
        const QString &user);

    /// Migrate a csync_journal to the new path, if necessary. Returns false on error
    static bool maybeMigrateDb(const QString &localPath, const QString &absoluteJournalPath);

    /// Given a sorted list of paths ending with '/', return whether or not the given path is within one of the paths of the list
    static bool findPathInSelectiveSyncList(const QStringList &list, const QString &path);

    // To verify that the record could be found check with SyncJournalFileRecord::isValid()
    [[nodiscard]] bool getFileRecord(const QString &filename, SyncJournalFileRecord *rec) { return getFileRecord(filename.toUtf8(), rec); }
    [[nodiscard]] bool getFileRecord(const QByteArray &filename, SyncJournalFileRecord *rec);
    [[nodiscard]] bool getFileRecordByE2eMangledName(const QString &mangledName, SyncJournalFileRecord *rec);
    [[nodiscard]] bool getFileRecordByInode(quint64 inode, SyncJournalFileRecord *rec);
    [[nodiscard]] bool getFileRecordsByFileId(const QByteArray &fileId, const std::function<void(const SyncJournalFileRecord &)> &rowCallback);
    [[nodiscard]] bool getFilesBelowPath(const QByteArray &path, const std::function<void(const SyncJournalFileRecord&)> &rowCallback);
    [[nodiscard]] bool listFilesInPath(const QByteArray &path, const std::function<void(const SyncJournalFileRecord&)> &rowCallback);
    [[nodiscard]] Result<void, QString> setFileRecord(const SyncJournalFileRecord &record);
    [[nodiscard]] bool getRootE2eFolderRecord(const QString &remoteFolderPath, SyncJournalFileRecord *rec);
    [[nodiscard]] bool listAllE2eeFoldersWithEncryptionStatusLessThan(const int status, const std::function<void(const SyncJournalFileRecord &)> &rowCallback);
    [[nodiscard]] bool findEncryptedAncestorForRecord(const QString &filename, SyncJournalFileRecord *rec);

    void keyValueStoreSet(const QString &key, QVariant value);
    [[nodiscard]] qint64 keyValueStoreGetInt(const QString &key, qint64 defaultValue);
    [[nodiscard]] QString keyValueStoreGetString(const QString &key, const QString &defaultValue = {});
    void keyValueStoreDelete(const QString &key);

    [[nodiscard]] bool deleteFileRecord(const QString &filename, bool recursively = false);
    [[nodiscard]] bool updateFileRecordChecksum(
        const QString &filename,
        const QByteArray &contentChecksum,
        const QByteArray &contentChecksumType);
    [[nodiscard]] bool updateLocalMetadata(const QString &filename,
        qint64 modtime, qint64 size, quint64 inode, const SyncJournalFileLockInfo &lockInfo);

    [[nodiscard]] bool hasFileIds(const QList<qint64> &fileIds);

    /// Return value for hasHydratedOrDehydratedFiles()
    struct HasHydratedDehydrated
    {
        bool hasHydrated = false;
        bool hasDehydrated = false;
    };

    /** Returns whether the item or any subitems are dehydrated */
    Optional<HasHydratedDehydrated> hasHydratedOrDehydratedFiles(const QByteArray &filename);

    bool exists();
    void walCheckpoint();

    [[nodiscard]] QString databaseFilePath() const;

    static qint64 getPHash(const QByteArray &);

    void setErrorBlacklistEntry(const SyncJournalErrorBlacklistRecord &item);
    void wipeErrorBlacklistEntry(const QString &file);
    void wipeErrorBlacklistCategory(SyncJournalErrorBlacklistRecord::Category category);
    [[nodiscard]] int wipeErrorBlacklist();
    int errorBlackListEntryCount();

    struct DownloadInfo
    {
        QString _tmpfile;
        QByteArray _etag;
        int _errorCount = 0;
        bool _valid = false;
    };
    struct UploadInfo
    {
        int _chunkUploadV1 = 0;
        uint _transferid = 0;
        qint64 _size = 0;
        qint64 _modtime = 0;
        int _errorCount = 0;
        bool _valid = false;
        QByteArray _contentChecksum;
        /**
         * Returns true if this entry refers to a chunked upload that can be continued.
         * (As opposed to a small file transfer which is stored in the db so we can detect the case
         * when the upload succeeded, but the connection was dropped before we got the answer)
         */
        [[nodiscard]] bool isChunked() const { return _transferid != 0; }
    };

    struct PollInfo
    {
        QString _file; // The relative path of a file
        QString _url; // the poll url. (This pollinfo is invalid if _url is empty)
        qint64 _modtime = 0LL; // The modtime of the file being uploaded
        qint64 _fileSize = 0LL;
    };

    DownloadInfo getDownloadInfo(const QString &file);
    void setDownloadInfo(const QString &file, const DownloadInfo &i);
    QVector<DownloadInfo> getAndDeleteStaleDownloadInfos(const QSet<QString> &keep);
    int downloadInfoCount();

    UploadInfo getUploadInfo(const QString &file);
    void setUploadInfo(const QString &file, const UploadInfo &i);
    // Return the list of transfer ids that were removed.
    QVector<uint> deleteStaleUploadInfos(const QSet<QString> &keep);

    SyncJournalErrorBlacklistRecord errorBlacklistEntry(const QString &);
    [[nodiscard]] bool deleteStaleErrorBlacklistEntries(const QSet<QString> &keep);

    /// Delete flags table entries that have no metadata correspondent
    void deleteStaleFlagsEntries();

    void avoidRenamesOnNextSync(const QString &path) { avoidRenamesOnNextSync(path.toUtf8()); }
    void avoidRenamesOnNextSync(const QByteArray &path);
    void setPollInfo(const PollInfo &);

    QVector<PollInfo> getPollInfos();

    enum SelectiveSyncListType {
        /** The black list is the list of folders that are unselected in the selective sync dialog.
         * For the sync engine, those folders are considered as if they were not there, so the local
         * folders will be deleted */
        SelectiveSyncBlackList = 1,
        /** When a shared folder has a size bigger than a configured size, it is by default not sync'ed
         * Unless it is in the white list, in which case the folder is sync'ed and all its children.
         * If a folder is both on the black and the white list, the black list wins */
        SelectiveSyncWhiteList = 2,
        /** List of big sync folders that have not been confirmed by the user yet and that the UI
         * should notify about */
        SelectiveSyncUndecidedList = 3,
        /** List of encrypted folders that will need to be removed from the blacklist when E2EE gets set up*/
        SelectiveSyncE2eFoldersToRemoveFromBlacklist = 4,
    };
    /* return the specified list from the database */
    QStringList getSelectiveSyncList(SelectiveSyncListType type, bool *ok);
    /* Write the selective sync list (remove all other entries of that list */
    void setSelectiveSyncList(SelectiveSyncListType type, const QStringList &list);

    QStringList addSelectiveSyncLists(SelectiveSyncListType type, const QString &path);

    QStringList removeSelectiveSyncLists(SelectiveSyncListType type, const QString &path);

    /**
     * Make sure that on the next sync fileName and its parents are discovered from the server.
     *
     * That means its metadata and, if it's a directory, its direct contents.
     *
     * Specifically, etag (md5 field) of fileName and all its parents are set to _invalid_.
     * That causes a metadata difference and a resulting discovery from the remote for the
     * affected folders.
     *
     * Since folders in the selective sync list will not be rediscovered (csync_ftw,
     * _csync_detect_update skip them), the _invalid_ marker will stay. And any
     * child items in the db will be ignored when reading a remote tree from the database.
     *
     * Any setFileRecord() call to affected directories before the next sync run will be
     * adjusted to retain the invalid etag via _etagStorageFilter.
     */
    void schedulePathForRemoteDiscovery(const QString &fileName) { schedulePathForRemoteDiscovery(fileName.toUtf8()); }
    void schedulePathForRemoteDiscovery(const QByteArray &fileName);

    /**
     * Wipe _etagStorageFilter. Also done implicitly on close().
     */
    void clearEtagStorageFilter();

    /**
     * Ensures full remote discovery happens on the next sync.
     *
     * Equivalent to calling schedulePathForRemoteDiscovery() for all files.
     */
    void forceRemoteDiscoveryNextSync();

    /* Because sqlite transactions are really slow, we encapsulate everything in big transactions
     * Commit will actually commit the transaction and create a new one.
     */
    void commit(const QString &context, bool startTrans = true);
    void commitIfNeededAndStartNewTransaction(const QString &context);

    /** Open the db if it isn't already.
     *
     * This usually creates some temporary files next to the db file, like
     * $dbfile-shm or $dbfile-wal.
     *
     * returns true if it could be opened or is currently opened.
     */
    bool open();

    /** Returns whether the db is currently opened. */
    bool isOpen();

    /** Close the database */
    void close();

    /**
     * Returns the checksum type for an id.
     */
    QByteArray getChecksumType(int checksumTypeId);

    /**
     * The data-fingerprint used to detect backup
     */
    void setDataFingerprint(const QByteArray &dataFingerprint);
    QByteArray dataFingerprint();


    // Conflict record functions

    /// Store a new or updated record in the database
    void setConflictRecord(const ConflictRecord &record);

    /// Retrieve a conflict record by path of the file with the conflict tag
    ConflictRecord conflictRecord(const QByteArray &path);

    /// Retrieve a conflict record by path of the file with the conflict tag
    ConflictRecord caseConflictRecordByBasePath(const QString &baseNamePath);

    /// Retrieve a conflict record by path of the file with the conflict tag
    ConflictRecord caseConflictRecordByPath(const QString &path);

    /// Return all paths of files with a conflict tag in the name and records in the db
    QByteArrayList caseClashConflictRecordPaths();

    /// Delete a conflict record by path of the file with the conflict tag
    void deleteConflictRecord(const QByteArray &path);

    /// Return all paths of files with a conflict tag in the name and records in the db
    QByteArrayList conflictRecordPaths();

    /** Find the base name for a conflict file name, using journal or name pattern
     *
     * The path must be sync-folder relative.
     *
     * Will return an empty string if it's not even a conflict file by pattern.
     */
    QByteArray conflictFileBaseName(const QByteArray &conflictName);

    /**
     * Delete any file entry. This will force the next sync to re-sync everything as if it was new,
     * restoring everyfile on every remote. If a file is there both on the client and server side,
     * it will be a conflict that will be automatically resolved if the file is the same.
     */
    void clearFileTable();

    /**
     * Set the 'ItemTypeVirtualFileDownload' to all the files that have the ItemTypeVirtualFile flag
     * within the directory specified path path
     *
     * The path "" marks everything.
     */
    void markVirtualFileForDownloadRecursively(const QByteArray &path);

    void setE2EeLockedFolder(const QByteArray &folderId, const QByteArray &folderToken);
    QByteArray e2EeLockedFolder(const QByteArray &folderId);
    QList<QPair<QByteArray, QByteArray>> e2EeLockedFolders();
    void deleteE2EeLockedFolder(const QByteArray &folderId);

    /** Grouping for all functions relating to pin states,
     *
     * Use internalPinStates() to get at them.
     */
    struct OCSYNC_EXPORT PinStateInterface
    {
        explicit PinStateInterface(SyncJournalDb *db)
            : _db(db)
        {
        }

        PinStateInterface(const PinStateInterface &) = delete;
        PinStateInterface(PinStateInterface &&) = delete;

        /**
         * Gets the PinState for the path without considering parents.
         *
         * If a path has no explicit PinState "Inherited" is returned.
         *
         * The path should not have a trailing slash.
         * It's valid to use the root path "".
         *
         * Returns none on db error.
         */
        Optional<PinState> rawForPath(const QByteArray &path);

        /**
         * Gets the PinState for the path after inheriting from parents.
         *
         * If the exact path has no entry or has an Inherited state,
         * the state of the closest parent path is returned.
         *
         * The path should not have a trailing slash.
         * It's valid to use the root path "".
         *
         * Never returns PinState::Inherited. If the root is "Inherited"
         * or there's an error, "AlwaysLocal" is returned.
         *
         * Returns none on db error.
         */
        Optional<PinState> effectiveForPath(const QByteArray &path);

        /**
         * Like effectiveForPath() but also considers subitem pin states.
         *
         * If the path's pin state and all subitem's pin states are identical
         * then that pin state will be returned.
         *
         * If some subitem's pin state is different from the path's state,
         * PinState::Inherited will be returned. Inherited isn't returned in
         * any other cases.
         *
         * It's valid to use the root path "".
         * Returns none on db error.
         */
        Optional<PinState> effectiveForPathRecursive(const QByteArray &path);

        /**
         * Sets a path's pin state.
         *
         * The path should not have a trailing slash.
         * It's valid to use the root path "".
         */
        void setForPath(const QByteArray &path, PinState state);

        /**
         * Wipes pin states for a path and below.
         *
         * Used when the user asks a subtree to have a particular pin state.
         * The path should not have a trailing slash.
         * The path "" wipes every entry.
         */
        void wipeForPathAndBelow(const QByteArray &path);

        /**
         * Returns list of all paths with their pin state as in the db.
         *
         * Returns nothing on db error.
         * Note that this will have an entry for "".
         */
        Optional<QVector<QPair<QByteArray, PinState>>> rawList();

        SyncJournalDb *_db;
    };
    friend struct PinStateInterface;

    /** Access to PinStates stored in the database.
     *
     * Important: Not all vfs plugins store the pin states in the database,
     * prefer to use Vfs::pinState() etc.
     */
    PinStateInterface internalPinStates();

    /**
     * Only used for auto-test:
     * when positive, will decrease the counter for every database operation.
     * reaching 0 makes the operation fails
     */
    int autotestFailCounter = -1;

public slots:
    /// Store a new or updated record in the database
    void setCaseConflictRecord(const OCC::ConflictRecord &record);

    /// Delete a case clash conflict record by path of the file with the conflict tag
    void deleteCaseClashConflictByPathRecord(const QString &path);

private:
    int getFileRecordCount();
    [[nodiscard]] bool ensureCorrectEncryptionStatus();
    [[nodiscard]] bool updateDatabaseStructure();
    [[nodiscard]] bool updateMetadataTableStructure();
    [[nodiscard]] bool updateErrorBlacklistTableStructure();
    [[nodiscard]] bool removeColumn(const QString &columnName);
    [[nodiscard]] bool hasDefaultValue(const QString &columnName);
    bool sqlFail(const QString &log, const SqlQuery &query);
    void commitInternal(const QString &context, bool startTrans = true);
    void startTransaction();
    void commitTransaction();
    QVector<QByteArray> tableColumns(const QByteArray &table);
    bool checkConnect();

    // Same as forceRemoteDiscoveryNextSync but without acquiring the lock
    void forceRemoteDiscoveryNextSyncLocked();

    // Returns the integer id of the checksum type
    //
    // Returns 0 on failure and for empty checksum types.
    [[nodiscard]] int mapChecksumType(const QByteArray &checksumType);

    SqlDatabase _db;
    QString _dbFile;
    QRecursiveMutex _mutex; // Public functions are protected with the mutex.
    QMap<QByteArray, int> _checksymTypeCache;
    int _transaction = 0;
    bool _metadataTableIsEmpty = false;

    /* Storing etags to these folders, or their parent folders, is filtered out.
     *
     * When schedulePathForRemoteDiscovery() is called some etags to _invalid_ in the
     * database. If this is done during a sync run, a later propagation job might
     * undo that by writing the correct etag to the database instead. This filter
     * will prevent this write and instead guarantee the _invalid_ etag stays in
     * place.
     *
     * The list is cleared on close() (end of sync run) and explicitly with
     * clearEtagStorageFilter() (start of sync run).
     *
     * The contained paths have a trailing /.
     */
    QList<QByteArray> _etagStorageFilter;

    /** The journal mode to use for the db.
     *
     * Typically WAL initially, but may be set to other modes via environment
     * variable, for specific filesystems, or when WAL fails in a particular way.
     */
    QByteArray _journalMode;

    PreparedSqlQueryManager _queryManager;

    friend class ::TestSyncJournalDB;
};

bool OCSYNC_EXPORT
operator==(const SyncJournalDb::DownloadInfo &lhs,
    const SyncJournalDb::DownloadInfo &rhs);
bool OCSYNC_EXPORT
operator==(const SyncJournalDb::UploadInfo &lhs,
    const SyncJournalDb::UploadInfo &rhs);

} // namespace OCC
#endif // SYNCJOURNALDB_H
