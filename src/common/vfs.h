/*
 * Copyright (C) by Christian Kamm <mail@ckamm.de>
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
#pragma once

#include "assert.h"
#include "ocsynclib.h"
#include "pinstate.h"
#include "result.h"
#include "syncfilestatus.h"

#include <QObject>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QUrl>
#include <QVersionNumber>

#include <memory>

typedef struct csync_file_stat_s csync_file_stat_t;

namespace OCC {

class Account;
typedef QSharedPointer<Account> AccountPtr;
class SyncJournalDb;
class VfsPrivate;
class SyncFileItem;

/** Collection of parameters for initializing a Vfs instance. */
struct OCSYNC_EXPORT VfsSetupParams
{
    VfsSetupParams() = default;

    explicit VfsSetupParams(const AccountPtr &account, const QUrl &baseUrl, bool groupInSidebar)
        : account(account)
        , _baseUrl(baseUrl)
        , _groupInSidebar(groupInSidebar)
    {
    }
    /** The full path to the folder on the local filesystem
     *
     * Always ends with /.
     */
    QString filesystemPath;

    /** The path to the synced folder on the account
     *
     * Always ends with /.
     */
    QString remotePath;

    /// Account url, credentials etc for network calls
    AccountPtr account;

    /** Access to the sync folder's database.
     *
     * Note: The journal must live at least until the Vfs::stop() call.
     */
    SyncJournalDb *journal = nullptr;

    /// Strings potentially passed on to the platform
    QString providerDisplayName;
    QString providerName;
    QVersionNumber providerVersion;

    /** when registering with the system we might use
     *  a different presentaton to identify the accounts
     */
    bool multipleAccountsRegistered = false;

    const QUrl &baseUrl() const
    {
        return _baseUrl;
    }

    bool groupInSidebar() const
    {
        return _groupInSidebar;
    }


private:
    QUrl _baseUrl;
    bool _groupInSidebar = false;
};

/** Interface describing how to deal with virtual/placeholder files.
 *
 * There are different ways of representing files locally that will only
 * be filled with data (hydrated) on demand. One such way would be suffixed
 * files, others could be FUSE based or use Windows CfAPI.
 *
 * This interface intends to decouple the sync algorithm and Folder from
 * the details of how a particular VFS solution works.
 *
 * An instance is usually created through a plugin via the createVfsFromPlugin()
 * function.
 */
class OCSYNC_EXPORT Vfs : public QObject
{
    Q_OBJECT

public:
    /** The kind of VFS in use (or no-VFS)
     *
     * Currently plugins and modes are one-to-one but that's not required.
     */
    enum Mode
    {
        Off,
        WithSuffix,
        WindowsCfApi,
    };
    Q_ENUM(Mode)
    enum class ConvertToPlaceholderResult {
        Error,
        Ok,
        Locked
    };
    Q_ENUM(ConvertToPlaceholderResult)

    static QString modeToString(Mode mode);
    static Optional<Mode> modeFromString(const QString &str);

    static Result<void, QString> checkAvailability(const QString &path, OCC::Vfs::Mode mode);

    enum class AvailabilityError
    {
        // Availability can't be retrieved due to db error
        DbError,
        // Availability not available since the item doesn't exist
        NoSuchItem,
    };
    using AvailabilityResult = Result<VfsItemAvailability, AvailabilityError>;

public:
    explicit Vfs(QObject* parent = nullptr);
    ~Vfs() override;

    virtual Mode mode() const = 0;

    /// For WithSuffix modes: the suffix (including the dot)
    virtual QString fileSuffix() const = 0;

    /// The fileName without fileSuffix
    /// TODO: better naming welcome
    virtual QString underlyingFileName(const QString &fileName) const;

    /// Access to the parameters the instance was start()ed with.
    const VfsSetupParams &params() const { return _setupParams; }

    /** Initializes interaction with the VFS provider.
     *
     * The plugin-specific work is done in startImpl().
     */
    void start(const VfsSetupParams &params);

    /// Stop interaction with VFS provider. Like when the client application quits.
    virtual void stop() = 0;

    /// Deregister the folder with the sync provider, like when a folder is removed.
    virtual void unregisterFolder() = 0;


    /** Whether the socket api should show pin state options
     *
     * Some plugins might provide alternate shell integration, making the normal
     * context menu actions redundant.
     */
    virtual bool socketApiPinStateActionsShown() const = 0;

    /** Return true when download of a file's data is currently ongoing.
     *
     * See also the beginHydrating() and doneHydrating() signals.
     */
    virtual bool isHydrating() const = 0;

    /// Create a new dehydrated placeholder. Called from PropagateDownload.
    virtual OC_REQUIRED_RESULT Result<void, QString> createPlaceholder(const SyncFileItem &item) = 0;

    /** Discovery hook: even unchanged files may need UPDATE_METADATA.
     *
     * For instance cfapi vfs wants local hydrated non-placeholder files to
     * become hydrated placeholder files.
     */
    virtual OC_REQUIRED_RESULT bool needsMetadataUpdate(const SyncFileItem &item) = 0;

    /// Determine whether the file at the given absolute path is a dehydrated placeholder.
    virtual OC_REQUIRED_RESULT bool isDehydratedPlaceholder(const QString &filePath) = 0;

    /** Similar to isDehydratedPlaceholder() but used from sync discovery.
     *
     * This function shall set stat->type if appropriate.
     * It may rely on stat->path and stat_data (platform specific data).
     *
     * Returning true means that type was fully determined.
     */
    virtual OC_REQUIRED_RESULT bool statTypeVirtualFile(csync_file_stat_t *stat, void *stat_data) = 0;

    /** Sets the pin state for the item at a path.
     *
     * The pin state is set on the item and for all items below it.
     *
     * Usually this would forward to setting the pin state flag in the db table,
     * but some vfs plugins will store the pin state in file attributes instead.
     *
     * relFilePath is relative to the sync folder. Can be "" for root folder.
     */
    virtual OC_REQUIRED_RESULT bool setPinState(const QString &relFilePath, PinState state) = 0;

    /** Returns the pin state of an item at a path.
     *
     * Usually backed by the db's effectivePinState() function but some vfs
     * plugins will override it to retrieve the state from elsewhere.
     *
     * relFilePath is relative to the sync folder. Can be "" for root folder.
     *
     * Returns none on retrieval error.
     */
    virtual OC_REQUIRED_RESULT Optional<PinState> pinState(const QString &relFilePath) = 0;

    /** Returns availability status of an item at a path.
     *
     * The availability is a condensed user-facing version of PinState. See
     * VfsItemAvailability for details.
     *
     * folderPath is relative to the sync folder. Can be "" for root folder.
     */
    virtual OC_REQUIRED_RESULT AvailabilityResult availability(const QString &folderPath) = 0;

public slots:
    /** Update in-sync state based on SyncFileStatusTracker signal.
     *
     * For some vfs plugins the icons aren't based on SocketAPI but rather on data shared
     * via the vfs plugin. The connection to SyncFileStatusTracker allows both to be based
     * on the same data.
     */
    virtual void fileStatusChanged(const QString &systemFileName, SyncFileStatus fileStatus) = 0;

signals:
    /// Emitted when a user-initiated hydration starts
    void beginHydrating();
    /// Emitted when the hydration ends
    void doneHydrating();
    /// start complete
    void started();

    /// we encountered an error
    void error(const QString &error);

protected:
    /** Update placeholder metadata during discovery.
     *
     * If the remote metadata changes, the local placeholder's metadata should possibly
     * change as well.
     */
    virtual OC_REQUIRED_RESULT Result<ConvertToPlaceholderResult, QString> updateMetadata(const SyncFileItem &item, const QString &filePath, const QString &replacesFile) = 0;

    /** Setup the plugin for the folder.
     *
     * For example, the VFS provider might monitor files to be able to start a file
     * hydration (download of a file's remote contents) when the user wants to open
     * it.
     *
     * Usually some registration needs to be done with the backend. This function
     * should take care of it if necessary.
     */
    virtual void startImpl(const VfsSetupParams &params) = 0;

    // Db-backed pin state handling. Derived classes may use it to implement pin states.
    bool setPinStateInDb(const QString &folderPath, PinState state);
    Optional<PinState> pinStateInDb(const QString &folderPath);
    AvailabilityResult availabilityInDb(const QString &folderPath);

    // the parameters passed to start()
    VfsSetupParams _setupParams;

    friend class OwncloudPropagator;
};

/// Check whether the plugin for the mode is available.
OCSYNC_EXPORT bool isVfsPluginAvailable(Vfs::Mode mode);

/// Return the best available VFS mode.
OCSYNC_EXPORT Vfs::Mode bestAvailableVfsMode();

/// Create a VFS instance for the mode, returns nullptr on failure.
OCSYNC_EXPORT std::unique_ptr<Vfs> createVfsFromPlugin(Vfs::Mode mode);

} // namespace OCC
