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

#include <QObject>
#include <QScopedPointer>
#include <QSharedPointer>

#include <memory>

#include "ocsynclib.h"
#include "result.h"
#include "syncfilestatus.h"
#include "pinstate.h"

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
    QString providerName;
    QString providerVersion;

    /** Whether native shell integration shall be enabled
     *
     * For some plugins that doesn't work well in tests.
     */
    bool enableShellIntegration = true;
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
    static QString modeToString(Mode mode);
    static Optional<Mode> modeFromString(const QString &str);

public:
    explicit Vfs(QObject* parent = nullptr);
    virtual ~Vfs();

    virtual Mode mode() const = 0;

    /// For WithSuffix modes: the suffix (including the dot)
    virtual QString fileSuffix() const = 0;


    /** Initializes interaction with the VFS provider.
     *
     * For example, the VFS provider might monitor files to be able to start a file
     * hydration (download of a file's remote contents) when the user wants to open
     * it.
     *
     * Usually some registration needs to be done with the backend. This function
     * should take care of it if necessary.
     */
    virtual void start(const VfsSetupParams &params) = 0;

    /// Stop interaction with VFS provider. Like when the client application quits.
    virtual void stop() = 0;

    /// Deregister the folder with the sync provider, like when a folder is removed.
    virtual void unregisterFolder() = 0;


    /** Return true when download of a file's data is currently ongoing.
     *
     * See also the beginHydrating() and doneHydrating() signals.
     */
    virtual bool isHydrating() const = 0;

    /** Update placeholder metadata during discovery.
     *
     * If the remote metadata changes, the local placeholder's metadata should possibly
     * change as well.
     *
     * Returning false and setting error indicates an error.
     */
    virtual bool updateMetadata(const QString &filePath, time_t modtime, quint64 size, const QByteArray &fileId, QString *error) = 0;

    /// Create a new dehydrated placeholder. Called from PropagateDownload.
    virtual void createPlaceholder(const QString &syncFolder, const SyncFileItem &item) = 0;

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
    virtual void convertToPlaceholder(
        const QString &filename,
        const SyncFileItem &item,
        const QString &replacesFile = QString()) = 0;

    /// Determine whether the file at the given absolute path is a dehydrated placeholder.
    virtual bool isDehydratedPlaceholder(const QString &filePath) = 0;

    /** Similar to isDehydratedPlaceholder() but used from sync discovery.
     *
     * This function shall set stat->type if appropriate.
     * It may rely on stat->path and stat_data (platform specific data).
     *
     * Returning true means that type was fully determined.
     */
    virtual bool statTypeVirtualFile(csync_file_stat_t *stat, void *stat_data) = 0;

    /** Sets the pin state for the item at a path.
     *
     * Usually this would forward to setting the pin state flag in the db table,
     * but some vfs plugins will store the pin state in file attributes instead.
     *
     * folderPath is relative to the sync folder.
     */
    virtual bool setPinState(const QString &folderPath, PinState state) = 0;

    /** Returns the pin state of an item at a path.
     *
     * Usually backed by the db's effectivePinState() function but some vfs
     * plugins will override it to retrieve the state from elsewhere.
     *
     * folderPath is relative to the sync folder.
     */
    virtual Optional<PinState> getPinState(const QString &folderPath) = 0;

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
};

class OCSYNC_EXPORT VfsDefaults : public Vfs
{
public:
    explicit VfsDefaults(QObject* parent = nullptr);

    // stores the params
    void start(const VfsSetupParams &params) override;

    // use the journal to back the pinstates
    bool setPinState(const QString &folderPath, PinState state) override;
    Optional<PinState> getPinState(const QString &folderPath) override;

    // access initial setup data
    const VfsSetupParams &params() const { return _setupParams; }

protected:
    VfsSetupParams _setupParams;
};

/// Implementation of Vfs for Vfs::Off mode - does nothing
class OCSYNC_EXPORT VfsOff : public VfsDefaults
{
    Q_OBJECT

public:
    VfsOff(QObject* parent = nullptr);
    virtual ~VfsOff();

    Mode mode() const override { return Vfs::Off; }

    QString fileSuffix() const override { return QString(); }

    void stop() override {}
    void unregisterFolder() override {}

    bool isHydrating() const override { return false; }

    bool updateMetadata(const QString &, time_t, quint64, const QByteArray &, QString *) override { return true; }
    void createPlaceholder(const QString &, const SyncFileItem &) override {}
    void convertToPlaceholder(const QString &, const SyncFileItem &, const QString &) override {}

    bool isDehydratedPlaceholder(const QString &) override { return false; }
    bool statTypeVirtualFile(csync_file_stat_t *, void *) override { return false; }

public slots:
    void fileStatusChanged(const QString &, SyncFileStatus) override {}
};

/// Check whether the plugin for the mode is available.
OCSYNC_EXPORT bool isVfsPluginAvailable(Vfs::Mode mode);

/// Return the best available VFS mode.
OCSYNC_EXPORT Vfs::Mode bestAvailableVfsMode();

/// Create a VFS instance for the mode, returns nullptr on failure.
OCSYNC_EXPORT std::unique_ptr<Vfs> createVfsFromPlugin(Vfs::Mode mode);

} // namespace OCC
