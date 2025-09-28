/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "ocsynclib.h"
#include "result.h"
#include "syncfilestatus.h"
#include "pinstate.h"

#include "common/remoteinfo.h"

#include <QObject>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QString>
#include <QMap>

#include <deque>
#include <memory>

using csync_file_stat_t = struct csync_file_stat_s;

namespace OCC {

class Account;
using AccountPtr = QSharedPointer<Account>;
class SyncJournalDb;
class VfsPrivate;
class SyncFileItem;
using SyncFileItemPtr = QSharedPointer<SyncFileItem>;

struct OCSYNC_EXPORT PlaceholderCreateInfo {
    QString name;
    std::wstring stdWStringName;
    QString fullPath;
    RemoteInfo parsedProperties;
};

/** Collection of parameters for initializing a Vfs instance. */
struct OCSYNC_EXPORT VfsSetupParams
{
    /** The full path to the folder on the local filesystem
     *
     * Always ends with /.
     */
    QString filesystemPath;

    // Folder display name in Windows Explorer
    QString displayName;

    // Folder alias
    QString alias;

    // Folder registry navigation Pane CLSID
    QString navigationPaneClsid;

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
    QString providerName;
    QString providerVersion;

    /** when registering with the system we might use
     *  a different presentaton to identify the accounts
     */
    bool multipleAccountsRegistered = false;
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
        XAttr,
    };
    Q_ENUM(Mode)
    enum class ConvertToPlaceholderResult {
        Error,
        Ok,
        Locked
    };
    Q_ENUM(ConvertToPlaceholderResult)

    enum UpdateMetadataType {
        DatabaseMetadata = 1 << 0,
        FileMetadata = 1 << 1,
        AllMetadata = DatabaseMetadata | FileMetadata,
    };

    Q_DECLARE_FLAGS(UpdateMetadataTypes, UpdateMetadataType)
    Q_FLAG(UpdateMetadataType)

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
    Q_ENUM(AvailabilityError)
    using AvailabilityResult = Result<VfsItemAvailability, AvailabilityError>;

    enum class AvailabilityRecursivity {
        RecursiveAvailability,
        NotRecursiveAvailability,
    };
    Q_ENUM(AvailabilityRecursivity)

public:
    explicit Vfs(QObject* parent = nullptr);
    ~Vfs() override;

    [[nodiscard]] virtual Mode mode() const = 0;

    /// For WithSuffix modes: the suffix (including the dot)
    [[nodiscard]] virtual QString fileSuffix() const = 0;

    /// Access to the parameters the instance was start()ed with.
    [[nodiscard]] const VfsSetupParams &params() const { return _setupParams; }


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
    [[nodiscard]] virtual bool socketApiPinStateActionsShown() const = 0;

    /** Return true when download of a file's data is currently ongoing.
     *
     * See also the beginHydrating() and doneHydrating() signals.
     */
    [[nodiscard]] virtual bool isHydrating() const = 0;

    /** Update placeholder metadata during discovery.
     *
     * If the remote metadata changes, the local placeholder's metadata should possibly
     * change as well.
     */
    [[nodiscard]] virtual OCC::Result<OCC::Vfs::ConvertToPlaceholderResult, QString> updateMetadata(const SyncFileItem &syncItem, const QString &filePath, const QString &replacesFile) = 0;

    [[nodiscard]] virtual Result<Vfs::ConvertToPlaceholderResult, QString> updatePlaceholderMarkInSync(const QString &filePath, const SyncFileItem &item) = 0;

    [[nodiscard]] virtual bool isPlaceHolderInSync(const QString &filePath) const = 0;

    /// Create a new dehydrated placeholder. Called from PropagateDownload.
    [[nodiscard]] virtual Result<void, QString> createPlaceholder(const SyncFileItem &item) = 0;

    /// Create a new dehydrated list of placeholders
    [[nodiscard]] virtual Result<void, QString> createPlaceholders(const QList<SyncFileItemPtr> &items) = 0;

    /** Convert a hydrated placeholder to a dehydrated one. Called from PropagateDownlaod.
     *
     * This is different from delete+create because preserving some file metadata
     * (like pin states) may be essential for some vfs plugins.
     */
    [[nodiscard]] virtual Result<void, QString> dehydratePlaceholder(const SyncFileItem &item) = 0;

    /** Discovery hook: even unchanged files may need UPDATE_METADATA.
     *
     * For instance cfapi vfs wants local hydrated non-placeholder files to
     * become hydrated placeholder files.
     */
    [[nodiscard]] virtual bool needsMetadataUpdate(const SyncFileItem &item) = 0;

    /** Convert a new file to a hydrated placeholder.
     *
     * Some VFS integrations expect that every file, including those that have all
     * the remote data, are "placeholders". This function is called by PropagateDownload
     * to convert newly downloaded, fully hydrated files into placeholders.
     *
     * Implementations must make sure that calling this function on a file that already
     * is a placeholder is acceptable.
     *
     * replacesFile can optionally contain a filesystem path to a placeholder that this
     * new placeholder shall supersede, for rename-replace actions with new downloads,
     * for example.
     */
    [[nodiscard]] virtual Result<Vfs::ConvertToPlaceholderResult, QString> convertToPlaceholder(const QString &filename,
                                                                                                    const SyncFileItem &item,
                                                                                                    const QString &replacesFile = {},
                                                                                                    UpdateMetadataTypes updateType = AllMetadata) = 0;

    /// Determine whether the file at the given absolute path is a dehydrated placeholder.
    [[nodiscard]] virtual bool isDehydratedPlaceholder(const QString &filePath) = 0;

    /** Similar to isDehydratedPlaceholder() but used from sync discovery.
     *
     * This function shall set stat->type if appropriate.
     * It may rely on stat->path and stat_data (platform specific data).
     *
     * Returning true means that type was fully determined.
     */
    [[nodiscard]] virtual bool statTypeVirtualFile(csync_file_stat_t *stat, void *stat_data) = 0;

    /** Sets the pin state for the item at a path.
     *
     * The pin state is set on the item and for all items below it.
     *
     * Usually this would forward to setting the pin state flag in the db table,
     * but some vfs plugins will store the pin state in file attributes instead.
     *
     * folderPath is relative to the sync folder. Can be "" for root folder.
     */
    [[nodiscard]] virtual bool setPinState(const QString &folderPath, PinState state) = 0;

    /** Returns the pin state of an item at a path.
     *
     * Usually backed by the db's effectivePinState() function but some vfs
     * plugins will override it to retrieve the state from elsewhere.
     *
     * folderPath is relative to the sync folder. Can be "" for root folder.
     *
     * Returns none on retrieval error.
     */
    [[nodiscard]] virtual Optional<PinState> pinState(const QString &folderPath) = 0;

    /** Returns availability status of an item at a path.
     *
     * The availability is a condensed user-facing version of PinState. See
     * VfsItemAvailability for details.
     *
     * folderPath is relative to the sync folder. Can be "" for root folder.
     */
    [[nodiscard]] virtual AvailabilityResult availability(const QString &folderPath, const AvailabilityRecursivity recursiveCheck) = 0;

public slots:
    /** Update in-sync state based on SyncFileStatusTracker signal.
     *
     * For some vfs plugins the icons aren't based on SocketAPI but rather on data shared
     * via the vfs plugin. The connection to SyncFileStatusTracker allows both to be based
     * on the same data.
     */
    virtual void fileStatusChanged(const QString &systemFileName, OCC::SyncFileStatus fileStatus) = 0;

signals:
    /// Emitted when a user-initiated hydration starts
    void beginHydrating();
    /// Emitted when the hydration ends
    void doneHydrating();
    // Emitted when hydration fails
    void failureHydrating(int errorCode, int statusCode, const QString &errorString, const QString &fileName);

protected:
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
};

/// Implementation of Vfs for Vfs::Off mode - does nothing
class OCSYNC_EXPORT VfsOff : public Vfs
{
    Q_OBJECT

public:
    VfsOff(QObject* parent = nullptr);
    ~VfsOff() override;

    [[nodiscard]] Mode mode() const override { return Vfs::Off; }

    [[nodiscard]] QString fileSuffix() const override { return QString(); }

    void stop() override {}
    void unregisterFolder() override {}

    [[nodiscard]] bool socketApiPinStateActionsShown() const override { return false; }
    [[nodiscard]] bool isHydrating() const override { return false; }

    OCC::Result<OCC::Vfs::ConvertToPlaceholderResult, QString> updateMetadata(const SyncFileItem &, const QString &, const QString &) override { return {OCC::Vfs::ConvertToPlaceholderResult::Ok}; }
    Result<Vfs::ConvertToPlaceholderResult, QString> updatePlaceholderMarkInSync(const QString &filePath, const SyncFileItem &item) override {Q_UNUSED(filePath) Q_UNUSED(item) return {QString{}};}
    [[nodiscard]] bool isPlaceHolderInSync(const QString &filePath) const override { Q_UNUSED(filePath) return true; }

    Result<void, QString> createPlaceholder(const SyncFileItem &) override { return {}; }
    Result<void, QString> createPlaceholders(const QList<SyncFileItemPtr> &) override { return {}; }

    Result<void, QString> dehydratePlaceholder(const SyncFileItem &) override { return {}; }
    Result<ConvertToPlaceholderResult, QString> convertToPlaceholder(const QString &, const SyncFileItem &, const QString &, const UpdateMetadataTypes) override { return ConvertToPlaceholderResult::Ok; }

    bool needsMetadataUpdate(const SyncFileItem &) override { return false; }
    bool isDehydratedPlaceholder(const QString &) override { return false; }
    bool statTypeVirtualFile(csync_file_stat_t *, void *) override { return false; }

    bool setPinState(const QString &, PinState) override { return true; }
    Optional<PinState> pinState(const QString &) override { return PinState::AlwaysLocal; }
    AvailabilityResult availability(const QString &, const AvailabilityRecursivity) override { return VfsItemAvailability::AlwaysLocal; }

public slots:
    void fileStatusChanged(const QString &, OCC::SyncFileStatus) override {}

protected:
    void startImpl(const VfsSetupParams &) override {}
};

/// Check whether the plugin for the mode is available.
OCSYNC_EXPORT bool isVfsPluginAvailable(Vfs::Mode mode);

/// Return the best available VFS mode.
OCSYNC_EXPORT Vfs::Mode bestAvailableVfsMode();

/// Create a VFS instance for the mode, returns nullptr on failure.
OCSYNC_EXPORT std::unique_ptr<Vfs> createVfsFromPlugin(Vfs::Mode mode);

} // namespace OCC

#define OCC_DEFINE_VFS_FACTORY(name, Type) \
    static_assert (std::is_base_of<OCC::Vfs, Type>::value, "Please define VFS factories only for OCC::Vfs subclasses"); \
    namespace { \
    void initPlugin() \
    { \
        OCC::Vfs::registerPlugin(QStringLiteral(name), []() -> OCC::Vfs * { return new (Type); }); \
    } \
    Q_COREAPP_STARTUP_FUNCTION(initPlugin) \
    }
