/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "folderman.h"
#include "configfile.h"
#include "folder.h"
#include "syncresult.h"
#include "theme.h"
#include "socketapi/socketapi.h"
#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "filesystem.h"
#include "lockwatcher.h"
#include "common/asserts.h"
#include "gui/systray.h"
#include <pushnotifications.h>
#include <syncengine.h>
#include "updatee2eefolderusersmetadatajob.h"

#ifdef Q_OS_MACOS
#include <CoreServices/CoreServices.h>
#include "common/utility_mac_sandbox.h"
#ifdef BUILD_FILE_PROVIDER_MODULE
#include "macOS/fileprovider.h"
#endif
#endif

#include <QMessageBox>
#include <QtCore>
#include <QMutableSetIterator>
#include <QSet>
#include <QNetworkProxy>

namespace {
constexpr auto settingsAccountsC = "Accounts";
constexpr auto settingsFoldersC = "Folders";
constexpr auto settingsFoldersWithPlaceholdersC = "FoldersWithPlaceholders";
constexpr auto settingsVersionC = "version";
constexpr auto maxFoldersVersion = 1;

int numberOfSyncJournals(const QString &path)
{
    return QDir(path).entryList({ QStringLiteral(".sync_*.db"), QStringLiteral("._sync_*.db") }, QDir::Hidden | QDir::Files).size();
}

}

namespace OCC {

Q_LOGGING_CATEGORY(lcFolderMan, "nextcloud.gui.folder.manager", QtInfoMsg)

FolderMan *FolderMan::_instance = nullptr;

FolderMan::FolderMan(QObject *parent)
    : QObject(parent)
    , _lockWatcher(new LockWatcher)
#ifdef Q_OS_WIN
    , _navigationPaneHelper(this)
#endif
{
    ASSERT(!_instance);
    _instance = this;

    _socketApi.reset(new SocketApi);

    ConfigFile cfg;
    std::chrono::milliseconds polltime = cfg.remotePollInterval();
    qCInfo(lcFolderMan) << "setting remote poll timer interval to" << polltime.count() << "msec";
    _etagPollTimer.setInterval(polltime.count());
    QObject::connect(&_etagPollTimer, &QTimer::timeout, this, &FolderMan::slotEtagPollTimerTimeout);
    _etagPollTimer.start();

    _startScheduledSyncTimer.setSingleShot(true);
    connect(&_startScheduledSyncTimer, &QTimer::timeout,
        this, &FolderMan::slotStartScheduledFolderSync);

    _timeScheduler.setInterval(5000);
    _timeScheduler.setSingleShot(false);
    connect(&_timeScheduler, &QTimer::timeout,
        this, &FolderMan::slotScheduleFolderByTime);
    _timeScheduler.start();

    connect(AccountManager::instance(), &AccountManager::removeAccountFolders,
        this, &FolderMan::slotRemoveFoldersForAccount);

    connect(AccountManager::instance(), &AccountManager::accountSyncConnectionRemoved,
        this, &FolderMan::slotAccountRemoved);

    connect(_lockWatcher.data(), &LockWatcher::fileUnlocked,
        this, &FolderMan::slotWatchedFileUnlocked);

    connect(this, &FolderMan::folderListChanged, this, &FolderMan::slotSetupPushNotifications);
}

FolderMan *FolderMan::instance()
{
    return _instance;
}

FolderMan::~FolderMan()
{
    qDeleteAll(_folderMap);
    _instance = nullptr;
}

const OCC::Folder::Map &FolderMan::map() const
{
    return _folderMap;
}

void FolderMan::unloadFolder(Folder *f)
{
    if (!f) {
        return;
    }

    _socketApi->slotUnregisterPath(f->alias());

    _folderMap.remove(f->alias());

    disconnect(f, &Folder::syncStarted,
        this, &FolderMan::slotFolderSyncStarted);
    disconnect(f, &Folder::syncFinished,
        this, &FolderMan::slotFolderSyncFinished);
    disconnect(f, &Folder::syncStateChange,
        this, &FolderMan::slotForwardFolderSyncStateChange);
    disconnect(f, &Folder::syncPausedChanged,
        this, &FolderMan::slotFolderSyncPaused);
    disconnect(&f->syncEngine().syncFileStatusTracker(), &SyncFileStatusTracker::fileStatusChanged,
        _socketApi.data(), &SocketApi::broadcastStatusPushMessage);
    disconnect(f, &Folder::watchedFileChangedExternally,
        &f->syncEngine().syncFileStatusTracker(), &SyncFileStatusTracker::slotPathTouched);
}

int FolderMan::unloadAndDeleteAllFolders()
{
    int cnt = 0;

    // clear the list of existing folders.
    Folder::MapIterator i(_folderMap);
    while (i.hasNext()) {
        i.next();
        Folder *f = i.value();
        unloadFolder(f);
        delete f;
        cnt++;
    }
    ASSERT(_folderMap.isEmpty());

    _lastSyncFolder = nullptr;
    _currentSyncFolder = nullptr;
    _scheduledFolders.clear();
    emit folderListChanged(_folderMap);
    emit scheduleQueueChanged();

    return cnt;
}

void FolderMan::registerFolderWithSocketApi(Folder *folder)
{
    if (!folder)
        return;
    if (!QDir(folder->path()).exists())
        return;

    // register the folder with the socket API
    if (folder->canSync())
        _socketApi->slotRegisterPath(folder->alias());
}

int FolderMan::setupFolders()
{
    Utility::registerUriHandlerForLocalEditing();

    unloadAndDeleteAllFolders();

    auto settings = ConfigFile::settingsWithGroup(QLatin1String("Accounts"));
    const auto accountsWithSettings = settings->childGroups();
    if (accountsWithSettings.isEmpty()) {
        const auto migratedFoldersCount = setupFoldersMigration();
        if (migratedFoldersCount > 0) {
            AccountManager::instance()->save(false); // don't save credentials, they had not been loaded from keychain
        }
        return migratedFoldersCount;
    }

    qCInfo(lcFolderMan) << "Setup folders from settings file";

    // this is done in Application::configVersionMigration
    QStringList skipSettingsKeys;
    backwardMigrationSettingsKeys(&skipSettingsKeys, &skipSettingsKeys);
    const auto accounts = AccountManager::instance()->accounts();
    for (const auto &account : accounts) {
        const auto id = account->account()->id();
        if (!accountsWithSettings.contains(id)) {
            continue;
        }
        settings->beginGroup(id);

        // The "backwardsCompatible" flag here is related to migrating old
        // database locations
        auto process = [&](const QString &groupName, const bool backwardsCompatible, const bool foldersWithPlaceholders) {
            settings->beginGroup(groupName);
            if (skipSettingsKeys.contains(settings->group())) {
                // Should not happen: bad container keys should have been deleted
                qCWarning(lcFolderMan) << "Folder structure" << groupName << "is too new, ignoring";
            } else {
                setupFoldersHelper(*settings, account, skipSettingsKeys, backwardsCompatible, foldersWithPlaceholders);
            }
            settings->endGroup();
        };

        process(QStringLiteral("Folders"), true, false);

        // See Folder::saveToSettings for details about why these exists.
        process(QStringLiteral("Multifolders"), false, false);
        process(QStringLiteral("FoldersWithPlaceholders"), false, true);

        settings->endGroup(); // <account>
    }

    emit folderListChanged(_folderMap);

    for (const auto folder : std::as_const(_folderMap)) {
        folder->processSwitchedToVirtualFiles();
    }

    return _folderMap.size();
}

void FolderMan::setupFoldersHelper(QSettings &settings, AccountStatePtr account, const QStringList &ignoreKeys, bool backwardsCompatible, bool foldersWithPlaceholders)
{
    const auto settingsChildGroups = settings.childGroups();
    for (const auto &folderAlias : settingsChildGroups) {
        // Skip folders with too-new version
        settings.beginGroup(folderAlias);
        if (ignoreKeys.contains(settings.group())) {
            qCInfo(lcFolderMan) << "Folder" << folderAlias << "is too new, ignoring";
            _additionalBlockedFolderAliases.insert(folderAlias);
            settings.endGroup();
            continue;
        }
        settings.endGroup();

        FolderDefinition folderDefinition;
        settings.beginGroup(folderAlias);
        if (FolderDefinition::load(settings, folderAlias, &folderDefinition)) {

#ifdef Q_OS_MACOS
            // macOS sandbox: Resolve the persisted security-scoped bookmark and
            // start accessing the resource BEFORE any filesystem operations on
            // the local sync folder path. The access handle will be transferred
            // to the Folder object once it is created via addFolderInternal().
            std::unique_ptr<Utility::MacSandboxPersistentAccess> securityScopedAccess;
            bool bookmarkRefreshed = false;
            if (!folderDefinition.securityScopedBookmarkData.isEmpty()) {
                securityScopedAccess = Utility::MacSandboxPersistentAccess::createFromBookmarkData(
                    folderDefinition.securityScopedBookmarkData);
                if (!securityScopedAccess || !securityScopedAccess->isValid()) {
                    qCWarning(lcFolderMan) << "Failed to restore security-scoped access for folder"
                                           << folderAlias << "at" << folderDefinition.localPath;
                    // Ensure we don't propagate an invalid or failed access handle.
                    securityScopedAccess.reset();
                } else if (securityScopedAccess->isStale()) {
                    // Bookmark still works but macOS flagged it as stale.
                    // Recreate it now while we have access so future launches
                    // won't run into problems. The updated data will be
                    // persisted when folder->saveToSettings() is called.
                    const auto refreshed = Utility::createSecurityScopedBookmarkData(folderDefinition.localPath);
                    if (!refreshed.isEmpty()) {
                        folderDefinition.securityScopedBookmarkData = refreshed;
                        bookmarkRefreshed = true;
                        qCInfo(lcFolderMan) << "Refreshed stale security-scoped bookmark for folder"
                                            << folderAlias;
                    }
                }
            } else {
                qCDebug(lcFolderMan) << "No security-scoped bookmark data for folder"
                                     << folderAlias;
            }
#endif

            auto defaultJournalPath = folderDefinition.defaultJournalPath(account->account());

            // Migration: Old settings don't have journalPath
            if (folderDefinition.journalPath.isEmpty()) {
                folderDefinition.journalPath = defaultJournalPath;
            }

            // Migration #2: journalPath might be absolute (in DataAppDir most likely) move it back to the root of local tree
            if (folderDefinition.journalPath.at(0) != QChar('.')) {
                QFile oldJournal(folderDefinition.journalPath);
                QFile oldJournalShm(folderDefinition.journalPath + QStringLiteral("-shm"));
                QFile oldJournalWal(folderDefinition.journalPath + QStringLiteral("-wal"));

                folderDefinition.journalPath = defaultJournalPath;

                socketApi()->slotUnregisterPath(folderAlias);
                auto settings = account->settings();

                auto journalFileMoveSuccess = true;
                // Due to db logic can't be sure which of these file exist.
                if (oldJournal.exists()) {
                    journalFileMoveSuccess &= oldJournal.rename(folderDefinition.localPath + "/" + folderDefinition.journalPath);
                }
                if (oldJournalShm.exists()) {
                    journalFileMoveSuccess &= oldJournalShm.rename(folderDefinition.localPath + "/" + folderDefinition.journalPath + QStringLiteral("-shm"));
                }
                if (oldJournalWal.exists()) {
                    journalFileMoveSuccess &= oldJournalWal.rename(folderDefinition.localPath + "/" + folderDefinition.journalPath + QStringLiteral("-wal"));
                }

                if (!journalFileMoveSuccess) {
                    qCWarning(lcFolderMan) << "Wasn't able to move 3.0 syncjournal database files to new location. One-time loss off sync settings possible.";
                } else {
                    qCInfo(lcFolderMan) << "Successfully migrated syncjournal database.";
                }

                auto vfs = createVfsFromPlugin(folderDefinition.virtualFilesMode);
                if (!vfs && folderDefinition.virtualFilesMode != Vfs::Off) {
                    qCWarning(lcFolderMan) << "Could not load plugin for mode" << folderDefinition.virtualFilesMode;
                }

                const auto folder = addFolderInternal(folderDefinition, account.data(), std::move(vfs));
#ifdef Q_OS_MACOS
                if (securityScopedAccess) {
                    folder->setSecurityScopedAccess(std::move(securityScopedAccess));
                }
#endif
                folder->saveToSettings();

                continue;
            }

            // Migration: ._ files sometimes can't be created.
            // So if the configured journalPath has a dot-underscore ("._sync_*.db")
            // but the current default doesn't have the underscore, switch to the
            // new default if no db exists yet.
            if (folderDefinition.journalPath.startsWith("._sync_")
                && defaultJournalPath.startsWith(".sync_")
                && !QFile::exists(folderDefinition.absoluteJournalPath())) {
                folderDefinition.journalPath = defaultJournalPath;
            }

            // Migration: If an old db is found, move it to the new name.
            if (backwardsCompatible) {
                SyncJournalDb::maybeMigrateDb(folderDefinition.localPath, folderDefinition.absoluteJournalPath());
            }

            const auto switchToVfs = isSwitchToVfsNeeded(folderDefinition);
            if (switchToVfs) {
                folderDefinition.virtualFilesMode = bestAvailableVfsMode();
            }

            auto vfs = createVfsFromPlugin(folderDefinition.virtualFilesMode);
            if (!vfs) {
                // TODO: Must do better error handling
                qFatal("Could not load plugin");
            }

            if (const auto folder = addFolderInternal(std::move(folderDefinition), account.data(), std::move(vfs))) {
#ifdef Q_OS_MACOS
                if (securityScopedAccess) {
                    folder->setSecurityScopedAccess(std::move(securityScopedAccess));
                }
                if (bookmarkRefreshed) {
                    folder->saveToSettings();
                }
#endif
                if (switchToVfs) {
                    folder->switchToVirtualFiles();
                }
                // Migrate the old "usePlaceholders" setting to the root folder pin state
                if (settings.value(QLatin1String(settingsVersionC), 1).toInt() == 1
                    && settings.value(QLatin1String("usePlaceholders"), false).toBool()) {
                    qCInfo(lcFolderMan) << "Migrate: From usePlaceholders to PinState::OnlineOnly";
                    folder->setRootPinState(PinState::OnlineOnly);
                }

                // Migration: Mark folders that shall be saved in a backwards-compatible way
                if (backwardsCompatible)
                    folder->setSaveBackwardsCompatible(true);
                if (foldersWithPlaceholders)
                    folder->setSaveInFoldersWithPlaceholders();

                scheduleFolder(folder);
                emit folderSyncStateChange(folder);
            }
        }
        settings.endGroup();
    }
}

int FolderMan::setupFoldersMigration()
{
    ConfigFile cfg;
    QDir storageDir(cfg.configPath());
    _folderConfigPath = cfg.configPath();
    auto configPath = _folderConfigPath;

#if !DISABLE_ACCOUNT_MIGRATION
    if (const auto legacyConfigPath = ConfigFile::discoveredLegacyConfigPath();!legacyConfigPath.isEmpty()) {
        configPath =  legacyConfigPath;
        qCInfo(lcFolderMan) << "Starting folder migration from legacy path:" << legacyConfigPath;
    }
#endif
    qCInfo(lcFolderMan) << "Setup folders from" << configPath;

    QDir dir(configPath);
    // We need to include hidden files just in case the alias starts with '.'
    dir.setFilter(QDir::Files | QDir::Hidden);
    // Exclude previous backed up configs e.g. oc.cfg.backup_20230831_133749_4.0.0
    // only need the current config in use by the legacy application
    const auto dirFiles = dir.entryList({"*.cfg"});

    // Migrate all folders for each account found in legacy config file(s)
    const auto legacyAccounts = AccountManager::instance()->accounts();
    for (const auto &fileName : dirFiles) {
        for (const auto &accountState : legacyAccounts) {
            const auto fullFilePath = dir.filePath(fileName);
            setupLegacyFolder(fullFilePath, accountState.data());
        }
    }

    emit folderListChanged(_folderMap);

    // return the number of valid folders.
    return _folderMap.size();
}

void FolderMan::backwardMigrationSettingsKeys(QStringList *deleteKeys, QStringList *ignoreKeys)
{
    auto settings = ConfigFile::settingsWithGroup(QLatin1String("Accounts"));

    auto processSubgroup = [&](const QString &name) {
        settings->beginGroup(name);
        const auto foldersVersion = settings->value(QLatin1String(settingsVersionC), 1).toInt();
        qCInfo(lcFolderMan) << "FolderDefinition::maxSettingsVersion:" << FolderDefinition::maxSettingsVersion();
        if (foldersVersion <= maxFoldersVersion) {
            const auto &childGroups = settings->childGroups();
            for (const auto &folderAlias : childGroups) {
                settings->beginGroup(folderAlias);
                const auto folderVersion = settings->value(QLatin1String(settingsVersionC), 1).toInt();
                if (folderVersion > FolderDefinition::maxSettingsVersion()) {
                    qCInfo(lcFolderMan) << "Ignoring folder:" << folderAlias << "version:" << folderVersion;
                    ignoreKeys->append(settings->group());
                }
                settings->endGroup();
            }
        } else {
            qCInfo(lcFolderMan) << "Ignoring group:" << name << "version:" << foldersVersion;
            deleteKeys->append(settings->group());
        }
        settings->endGroup();
    };

    const auto settingsChildGroups = settings->childGroups();
    for (const auto &accountId : settingsChildGroups) {
        settings->beginGroup(accountId);
        processSubgroup("Folders");
        processSubgroup("Multifolders");
        processSubgroup("FoldersWithPlaceholders");
        settings->endGroup();
    }
}

bool FolderMan::ensureJournalGone(const QString &journalDbFile)
{
    // remove the old journal file
    while (QFile::exists(journalDbFile) && !QFile::remove(journalDbFile)) {
        qCWarning(lcFolderMan) << "Could not remove old db file at" << journalDbFile;
        int ret = QMessageBox::warning(nullptr, tr("Could not reset folder state"),
            tr("An old sync journal \"%1\" was found, "
               "but could not be removed. Please make sure "
               "that no application is currently using it.")
                .arg(QDir::fromNativeSeparators(QDir::cleanPath(journalDbFile))),
            QMessageBox::Retry | QMessageBox::Abort);
        if (ret == QMessageBox::Abort) {
            return false;
        }
    }
    return true;
}

#define SLASH_TAG QLatin1String("__SLASH__")
#define BSLASH_TAG QLatin1String("__BSLASH__")
#define QMARK_TAG QLatin1String("__QMARK__")
#define PERCENT_TAG QLatin1String("__PERCENT__")
#define STAR_TAG QLatin1String("__STAR__")
#define COLON_TAG QLatin1String("__COLON__")
#define PIPE_TAG QLatin1String("__PIPE__")
#define QUOTE_TAG QLatin1String("__QUOTE__")
#define LT_TAG QLatin1String("__LESS_THAN__")
#define GT_TAG QLatin1String("__GREATER_THAN__")
#define PAR_O_TAG QLatin1String("__PAR_OPEN__")
#define PAR_C_TAG QLatin1String("__PAR_CLOSE__")

QString FolderMan::escapeAlias(const QString &alias)
{
    QString a(alias);

    a.replace(QLatin1Char('/'), SLASH_TAG);
    a.replace(QLatin1Char('\\'), BSLASH_TAG);
    a.replace(QLatin1Char('?'), QMARK_TAG);
    a.replace(QLatin1Char('%'), PERCENT_TAG);
    a.replace(QLatin1Char('*'), STAR_TAG);
    a.replace(QLatin1Char(':'), COLON_TAG);
    a.replace(QLatin1Char('|'), PIPE_TAG);
    a.replace(QLatin1Char('"'), QUOTE_TAG);
    a.replace(QLatin1Char('<'), LT_TAG);
    a.replace(QLatin1Char('>'), GT_TAG);
    a.replace(QLatin1Char('['), PAR_O_TAG);
    a.replace(QLatin1Char(']'), PAR_C_TAG);
    return a;
}

SocketApi *FolderMan::socketApi()
{
    return this->_socketApi.data();
}

QString FolderMan::unescapeAlias(const QString &alias)
{
    QString a(alias);

    a.replace(SLASH_TAG, QLatin1String("/"));
    a.replace(BSLASH_TAG, QLatin1String("\\"));
    a.replace(QMARK_TAG, QLatin1String("?"));
    a.replace(PERCENT_TAG, QLatin1String("%"));
    a.replace(STAR_TAG, QLatin1String("*"));
    a.replace(COLON_TAG, QLatin1String(":"));
    a.replace(PIPE_TAG, QLatin1String("|"));
    a.replace(QUOTE_TAG, QLatin1String("\""));
    a.replace(LT_TAG, QLatin1String("<"));
    a.replace(GT_TAG, QLatin1String(">"));
    a.replace(PAR_O_TAG, QLatin1String("["));
    a.replace(PAR_C_TAG, QLatin1String("]"));

    return a;
}

void FolderMan::setupLegacyFolder(const QString &fileNamePath, AccountState *accountState)
{
    qCInfo(lcFolderMan) << "  ` -> setting up:" << fileNamePath;
    QString escapedFileNamePath(fileNamePath);
    // check the unescaped variant (for the case when the filename comes out
    // of the directory listing). If the file does not exist, escape the
    // file and try again.
    QFileInfo cfgFile(fileNamePath);

    if (!cfgFile.exists()) {
        // try the escaped variant.
        escapedFileNamePath = escapeAlias(fileNamePath);
        cfgFile.setFile(_folderConfigPath, escapedFileNamePath);
    }
    if (!cfgFile.isReadable()) {
        qCWarning(lcFolderMan) << "Cannot read folder definition for alias " << cfgFile.filePath();
        return;
    }

    QSettings settings(escapedFileNamePath, QSettings::IniFormat);
    qCInfo(lcFolderMan) << "    -> file path: " << settings.fileName();

    // Check if the filename is equal to the group setting. If not, use the group
    // name as an alias.
    const auto groups = settings.childGroups();
    if (groups.isEmpty()) {
        qCWarning(lcFolderMan) << "empty file:" << cfgFile.filePath();
        return;
    }

    if (!accountState) {
        qCCritical(lcFolderMan) << "can't create folder without an account";
        return;
    }

    auto migrateFoldersGroup = [&](const QString &folderGroupName) {
        const auto childGroups = settings.childGroups();
        if (childGroups.isEmpty()) {
            qCDebug(lcFolderMan) << "There are no" << folderGroupName << "to migrate from account" <<  accountState->account()->id();
            return;
        }
        for (const auto &alias : childGroups) {
            settings.beginGroup(alias);
            qCDebug(lcFolderMan) << "try to migrate" << folderGroupName << "alias:" << alias;

            const auto path = settings.value(QLatin1String("localPath")).toString();
            const auto targetPath = settings.value(QLatin1String("targetPath")).toString();
            const auto journalPath = settings.value(QLatin1String("journalPath")).toString();
            const auto paused = settings.value(QLatin1String("paused"), false).toBool();
            const auto ignoreHiddenFiles = settings.value(QLatin1String("ignoreHiddenFiles"), false).toBool();
            const auto virtualFilesMode = settings.value(QLatin1String("virtualFilesMode"), false).toString();

            if (path.isEmpty()) {
                qCDebug(lcFolderMan) << "localPath is empty";
                settings.endGroup();
                continue;
            }

            if (targetPath.isEmpty()) {
                qCDebug(lcFolderMan) << "targetPath is empty";
                settings.endGroup();
                continue;
            }

            if (journalPath.isEmpty()) {
                qCDebug(lcFolderMan) << "journalPath is empty";
                settings.endGroup();
                continue;
            }

            qCDebug(lcFolderMan) << folderGroupName << "located at" << path;

            FolderDefinition folderDefinition;
            folderDefinition.alias = alias;
            folderDefinition.localPath = path;
            folderDefinition.targetPath = targetPath;
            folderDefinition.journalPath = journalPath;
            folderDefinition.paused = paused;
            folderDefinition.ignoreHiddenFiles = ignoreHiddenFiles;

            if (const auto vfsMode = Vfs::modeFromString(virtualFilesMode)) {
                folderDefinition.virtualFilesMode = *vfsMode;
            } else {
                qCWarning(lcFolderMan) << "Unknown virtualFilesMode:" << virtualFilesMode << "assuming 'off'";
            }

            qCDebug(lcFolderMan) << "folderDefinition.alias" << folderDefinition.alias;
            qCDebug(lcFolderMan) << "folderDefinition.virtualFilesMode" << folderDefinition.virtualFilesMode;

#ifdef Q_OS_MACOS
            // macOS sandbox: Legacy configs won't have bookmark data yet.
            // Try to create one now — this will only succeed if the app
            // currently has access (e.g. first migration run right after
            // the user granted access via QFileDialog).
            if (folderDefinition.securityScopedBookmarkData.isEmpty()) {
                folderDefinition.securityScopedBookmarkData = Utility::createSecurityScopedBookmarkData(folderDefinition.localPath);
            }
#endif

            auto vfs = createVfsFromPlugin(folderDefinition.virtualFilesMode);
            if (!vfs && folderDefinition.virtualFilesMode != Vfs::Off) {
                qCWarning(lcFolderMan) << "Could not load plugin for mode" << folderDefinition.virtualFilesMode;
            }

            if (const auto folder = addFolderInternal(folderDefinition, accountState, std::move(vfs))) {
                auto ok = true;
                auto legacyBlacklist = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList,
                                                                                 &ok);
                if (!ok) {
                    qCInfo(lcFolderMan) << "There was a problem retrieving the database selective sync for " << folder;
                }

                legacyBlacklist << settings.value(QLatin1String("blackList")).toStringList();
                if (!legacyBlacklist.isEmpty()) {
                    qCInfo(lcFolderMan) << "Legacy selective sync list found:" << legacyBlacklist;
                    for (const auto &legacyFolder : std::as_const(legacyBlacklist)) {
                        folder->migrateBlackListPath(legacyFolder);
                    }
                    settings.remove(QLatin1String("blackList"));
                }

                folder->saveToSettings();

                qCInfo(lcFolderMan) << "Migrated!" << folder->path();
                settings.sync();

                if (!folder) {
                    continue;
                }

                scheduleFolder(folder);
                emit folderSyncStateChange(folder);

#ifdef Q_OS_WIN
                Utility::migrateFavLink(folder->cleanPath());
#endif
            }
            settings.endGroup(); // folder alias
        }
    };

    settings.beginGroup(settingsAccountsC);
    qCDebug(lcFolderMan) << "try to migrate accountId:" << accountState->account()->id();
    settings.beginGroup(accountState->account()->id());

    settings.beginGroup(settingsFoldersWithPlaceholdersC);
    migrateFoldersGroup(settingsFoldersWithPlaceholdersC);
#ifdef Q_OS_WIN
    _navigationPaneHelper.scheduleUpdateCloudStorageRegistry();
#endif
    settings.endGroup();

    settings.beginGroup(settingsFoldersC);
    migrateFoldersGroup(settingsFoldersC);
    settings.endGroup();

    settings.endGroup();
    settings.endGroup();
    return;
}

void FolderMan::slotFolderSyncPaused(Folder *f, bool paused)
{
    if (!f) {
        qCCritical(lcFolderMan) << "slotFolderSyncPaused called with empty folder";
        return;
    }

    if (!paused) {
        _disabledFolders.remove(f);
        scheduleFolder(f);
    } else {
        _disabledFolders.insert(f);
    }
}

void FolderMan::slotFolderCanSyncChanged()
{
    auto *f = qobject_cast<Folder *>(sender());
     ASSERT(f);
    if (f->canSync()) {
        _socketApi->slotRegisterPath(f->alias());
    } else {
        _socketApi->slotUnregisterPath(f->alias());
    }
}

Folder *FolderMan::folder(const QString &alias)
{
    if (!alias.isEmpty()) {
        if (_folderMap.contains(alias)) {
            return _folderMap[alias];
        }
    }
    return nullptr;
}

void FolderMan::scheduleAllFolders()
{
    const auto folderMapValues = _folderMap.values();
    for (const auto f : folderMapValues) {
        if (f && f->canSync()) {
            scheduleFolder(f);
        }
    }
}

void FolderMan::forceSyncForFolder(Folder *folder)
{
    // Terminate and reschedule any running sync
    for (const auto folderInMap : map()) {
        if (folderInMap->isSyncRunning()) {
            folderInMap->slotTerminateSync();
            scheduleFolder(folderInMap);
        }
    }

    folder->slotWipeErrorBlacklist(); // issue #6757
    folder->setSyncPaused(false);

    // Insert the selected folder at the front of the queue
    scheduleFolderNext(folder);
}

void FolderMan::removeE2eFiles(const AccountPtr &account) const
{
    Q_ASSERT(!account->e2e()->isInitialized());
    for (const auto folder : map()) {
        if(folder->accountState()->account()->id() == account->id()) {
            folder->removeLocalE2eFiles();
        }
    }
}

void FolderMan::slotScheduleAppRestart()
{
    _appRestartRequired = true;
    qCInfo(lcFolderMan) << "Application restart requested!";
}

void FolderMan::slotSyncOnceFileUnlocks(const QString &path)
{
    _lockWatcher->addFile(path);
}

/*
  * if a folder wants to be synced, it calls this slot and is added
  * to the queue. The slot to actually start a sync is called afterwards.
  */
void FolderMan::scheduleFolder(Folder *f)
{
    if (!f) {
        qCCritical(lcFolderMan) << "slotScheduleSync called with null folder";
        return;
    }
    auto alias = f->alias();

    qCInfo(lcFolderMan) << "Schedule folder " << alias << " to sync!";

    auto syncAgainDelay = std::chrono::seconds(0);
    if (f->consecutiveFailingSyncs() > 2 && f->consecutiveFailingSyncs() <= 4) {
        syncAgainDelay = std::chrono::seconds(10);
    } else if (f->consecutiveFailingSyncs() > 4 && f->consecutiveFailingSyncs() <= 6) {
        syncAgainDelay = std::chrono::seconds(30);
    } else if (f->consecutiveFailingSyncs() > 6) {
        syncAgainDelay = std::chrono::seconds(60);
    }

    if (!_scheduledFolders.contains(f)) {
        if (!f->canSync()) {
            qCInfo(lcFolderMan) << "Folder is not ready to sync, not scheduled!";
            _socketApi->slotUpdateFolderView(f);
            return;
        }

        if (syncAgainDelay == std::chrono::seconds(0)) {
            f->prepareToSync();
            emit folderSyncStateChange(f);
            _scheduledFolders.enqueue(f);
            emit scheduleQueueChanged();
            startScheduledSyncSoon();
        } else {
            qCWarning(lcFolderMan()) << "going to delay the next sync run due to too many synchronization errors" << syncAgainDelay;
            QTimer::singleShot(syncAgainDelay, this, [this, f] () {
                f->prepareToSync();
                emit folderSyncStateChange(f);
                _scheduledFolders.enqueue(f);
                emit scheduleQueueChanged();
                startScheduledSyncSoon();
            });
        }
    } else {
        qCInfo(lcFolderMan) << "Sync for folder " << alias << " already scheduled, do not enqueue!";
        if (syncAgainDelay == std::chrono::seconds(0)) {
            startScheduledSyncSoon();
        } else {
            qCWarning(lcFolderMan()) << "going to delay the next sync run due to too many synchronization errors" << syncAgainDelay;
            QTimer::singleShot(syncAgainDelay, this, [this] () {
                startScheduledSyncSoon();
            });
        }
    }
}

void FolderMan::scheduleFolderForImmediateSync(Folder *f)
{
    _nextSyncShouldStartImmediately = true;
    scheduleFolder(f);
}

void FolderMan::scheduleFolderNext(Folder *f)
{
    auto alias = f->alias();
    qCInfo(lcFolderMan) << "Schedule folder " << alias << " to sync! Front-of-queue.";

    if (!f->canSync()) {
        qCInfo(lcFolderMan) << "Folder is not ready to sync, not scheduled!";
        return;
    }

    _scheduledFolders.removeAll(f);

    f->prepareToSync();
    emit folderSyncStateChange(f);
    _scheduledFolders.prepend(f);
    emit scheduleQueueChanged();

    startScheduledSyncSoon();
}

void FolderMan::slotScheduleETagJob(const QString & /*alias*/, RequestEtagJob *job)
{
    QObject::connect(job, &QObject::destroyed, this, &FolderMan::slotEtagJobDestroyed);
    QMetaObject::invokeMethod(this, "slotRunOneEtagJob", Qt::QueuedConnection);
    // maybe: add to queue
}

void FolderMan::slotEtagJobDestroyed(QObject * /*o*/)
{
    // _currentEtagJob is automatically cleared
    // maybe: remove from queue
    QMetaObject::invokeMethod(this, "slotRunOneEtagJob", Qt::QueuedConnection);
}

void FolderMan::slotRunOneEtagJob()
{
    if (_currentEtagJob.isNull()) {
        Folder *folder = nullptr;
        for (const auto f : std::as_const(_folderMap)) {
            if (f->etagJob()) {
                // Caveat: always grabs the first folder with a job, but we think this is Ok for now and avoids us having a separate queue.
                _currentEtagJob = f->etagJob();
                folder = f;
                break;
            }
        }
        if (_currentEtagJob.isNull()) {
            //qCDebug(lcFolderMan) << "No more remote ETag check jobs to schedule.";

            /* now it might be a good time to check for restarting... */
            if (!isAnySyncRunning() && _appRestartRequired) {
                restartApplication();
            }
        } else {
            qCDebug(lcFolderMan) << "Scheduling" << folder->remoteUrl().toString() << "to check remote ETag";
            _currentEtagJob->start(); // on destroy/end it will continue the queue via slotEtagJobDestroyed
        }
    }
}

void FolderMan::slotAccountStateChanged()
{
    auto *accountState = qobject_cast<AccountState *>(sender());
    if (!accountState) {
        return;
    }
    QString accountName = accountState->account()->displayName();

    if (accountState->isConnected()) {
        qCInfo(lcFolderMan) << "Account" << accountName << "connected, scheduling its folders";

        const auto folderMapValues = _folderMap.values();
        for (const auto f : folderMapValues) {
            if (f
                && f->canSync()
                && f->accountState() == accountState) {
                scheduleFolder(f);
            }
        }
    } else {
        qCInfo(lcFolderMan) << "Account" << accountName << "disconnected or paused, "
                                                           "terminating or descheduling sync folders";

        const auto folderValues = _folderMap.values();
        for (const auto f : folderValues) {
            if (f
                && f->isSyncRunning()
                && f->accountState() == accountState) {
                f->slotTerminateSync();
            }
        }

        QMutableListIterator<Folder *> it(_scheduledFolders);
        while (it.hasNext()) {
            Folder *f = it.next();
            if (f->accountState() == accountState) {
                it.remove();
            }
        }
        emit scheduleQueueChanged();
    }
}

// only enable or disable foldermans will schedule and do syncs.
// this is not the same as Pause and Resume of folders.
void FolderMan::setSyncEnabled(bool enabled)
{
    if (!_syncEnabled && enabled && !_scheduledFolders.isEmpty()) {
        // We have things in our queue that were waiting for the connection to come back on.
        startScheduledSyncSoon();
    }
    _syncEnabled = enabled;
    // force a redraw in case the network connect status changed
    emit folderSyncStateChange(nullptr);
}

void FolderMan::startScheduledSyncSoon()
{
    if (_startScheduledSyncTimer.isActive()) {
        return;
    }
    if (_scheduledFolders.empty()) {
        return;
    }
    if (isAnySyncRunning()) {
        return;
    }

    qint64 msDelay = 100; // 100ms minimum delay
    qint64 msSinceLastSync = 0;

    // Require a pause based on the duration of the last sync run.
    if (Folder *lastFolder = _lastSyncFolder) {
        msSinceLastSync = lastFolder->msecSinceLastSync().count();

        //  1s   -> 1.5s pause
        // 10s   -> 5s pause
        //  1min -> 12s pause
        //  1h   -> 90s pause
        qint64 pause = qSqrt(lastFolder->msecLastSyncDuration().count()) / 20.0 * 1000.0;
        msDelay = qMax(msDelay, pause);
    }

    // Delays beyond one minute seem too big, particularly since there
    // could be things later in the queue that shouldn't be punished by a
    // long delay!
    msDelay = qMin(msDelay, 60 * 1000ll);

    // Time since the last sync run counts against the delay
    msDelay = qMax(1ll, msDelay - msSinceLastSync);

    if (_nextSyncShouldStartImmediately) {
        _nextSyncShouldStartImmediately = false;
        qCInfo(lcFolderMan) << "Next sync is marked to start immediately, so setting the delay to '0'";
        msDelay = 0;
    }

    qCInfo(lcFolderMan) << "Starting the next scheduled sync in" << (msDelay / 1000) << "seconds";
    _startScheduledSyncTimer.start(msDelay);
}

/*
  * slot to start folder syncs.
  * It is either called from the slot where folders enqueue themselves for
  * syncing or after a folder sync was finished.
  */
void FolderMan::slotStartScheduledFolderSync()
{
    if (isAnySyncRunning()) {
        for (auto f : std::as_const(_folderMap)) {
            if (f->isSyncRunning())
                qCInfo(lcFolderMan) << "Currently folder " << f->remoteUrl().toString() << " is running, wait for finish!";
        }
        return;
    }

    if (!_syncEnabled) {
        qCInfo(lcFolderMan) << "FolderMan: Syncing is disabled, no scheduling.";
        return;
    }

    qCDebug(lcFolderMan) << "folderQueue size: " << _scheduledFolders.count();
    if (_scheduledFolders.isEmpty()) {
        return;
    }

    // Find the first folder in the queue that can be synced.
    Folder *folder = nullptr;
    while (!_scheduledFolders.isEmpty()) {
        Folder *g = _scheduledFolders.dequeue();
        if (g->canSync()) {
            folder = g;
            break;
        }
    }

    emit scheduleQueueChanged();

    // Start syncing this folder!
    if (folder) {
        // Safe to call several times, and necessary to try again if
        // the folder path didn't exist previously.
        folder->registerFolderWatcher();
        registerFolderWithSocketApi(folder);

        _currentSyncFolder = folder;
        folder->startSync(QStringList());
    }
}

bool FolderMan::pushNotificationsFilesReady(const AccountPtr &account)
{
    const auto pushNotifications = account->pushNotifications();
    const auto pushFilesAvailable = account->capabilities().availablePushNotifications() & PushNotificationType::Files;

    return pushFilesAvailable && pushNotifications && pushNotifications->isReady();
}

bool FolderMan::isSwitchToVfsNeeded(const FolderDefinition &folderDefinition) const
{
    auto result = false;
    if (!DISABLE_VIRTUAL_FILES_SYNC_FOLDER &&
            ENFORCE_VIRTUAL_FILES_SYNC_FOLDER &&
            folderDefinition.virtualFilesMode != bestAvailableVfsMode() &&
            folderDefinition.virtualFilesMode == Vfs::Off &&
            OCC::Theme::instance()->showVirtualFilesOption()) {
        result = true;
    }

    return result;
}

void FolderMan::slotEtagPollTimerTimeout()
{
    qCInfo(lcFolderMan) << "Etag poll timer timeout";

    const auto folderMapValues = _folderMap.values();

    qCInfo(lcFolderMan) << "Folders to sync:" << folderMapValues.size();

    QList<Folder *> foldersToRun;

    // Some folders need not to be checked because they use the push notifications
    std::copy_if(folderMapValues.begin(), folderMapValues.end(), std::back_inserter(foldersToRun), [this](Folder *folder) -> bool {
        const auto account = folder->accountState()->account();
        return !pushNotificationsFilesReady(account);
    });

    qCInfo(lcFolderMan) << "Number of folders that don't use push notifications:" << foldersToRun.size();

    runEtagJobsIfPossible(foldersToRun);

#ifdef BUILD_FILE_PROVIDER_MODULE
    // Signal the File Provider working set about remote changes
    // This must be independent of sync folder configuration since File Provider
    // can operate without traditional sync folders
    qCInfo(lcFolderMan) << "Checking root folder ETags for file provider domains.";
    const auto accounts = AccountManager::instance()->accounts();

    for (const auto &accountState : accounts) {
        const auto account = accountState->account();

        // Skip accounts that don't have a File Provider domain
        if (!Mac::FileProvider::instance()->domainManager()->domainForAccount(account.data())) {
            qCDebug(lcFolderMan) << "Account" << account->displayName() << "has no file provider domain, skipping.";
            continue;
        }

        // Skip accounts that use push notifications - they get real-time updates
        if (pushNotificationsFilesReady(account)) {
            qCDebug(lcFolderMan) << "Account" << account->displayName() << "uses push notifications, skipping ETag check";
            continue;
        }

        // For accounts using polling, check the root folder ETag
        qCInfo(lcFolderMan) << "Fetching root ETag for file provider domain of account:" << account->displayName();

        auto *etagJob = new RequestEtagJob(account, QStringLiteral("/"), this);
        etagJob->setTimeout(60 * 1000);

        connect(etagJob, &RequestEtagJob::etagRetrieved, this,
            [account](const QByteArray &etag, const QDateTime &) {
                qCDebug(lcFolderMan) << "Root ETag retrieved for account" << account->displayName() << ":" << etag;

                // Check if ETag has changed
                const auto lastEtag = account->lastRootETag();

                if (lastEtag != etag) {
                    qCInfo(lcFolderMan) << "Root ETag changed for" << account->displayName()
                                        << "from" << lastEtag << "to" << etag
                                        << ", signaling file provider domain.";

                    // Store new ETag in the account
                    account->setLastRootETag(etag);

                    // Signal File Provider about remote changes
                    Mac::FileProvider::instance()->domainManager()->signalEnumeratorChanged(account.data());
                } else {
                    qCDebug(lcFolderMan) << "Root ETag unchanged for account" 
                                         << account->displayName();
                }
            });

        etagJob->start();
    }
#endif
}

void FolderMan::runEtagJobsIfPossible(const QList<Folder *> &folderMap)
{
    for (auto folder : folderMap) {
        runEtagJobIfPossible(folder);
    }
}

void FolderMan::runEtagJobIfPossible(Folder *folder)
{
    const ConfigFile cfg;
    const auto polltime = cfg.remotePollInterval();

    qCInfo(lcFolderMan) << "Run etag job on folder" << folder;

    if (!folder) {
        return;
    }
    if (folder->isSyncRunning()) {
        qCInfo(lcFolderMan) << "Can not run etag job: Sync is running";
        return;
    }
    if (_scheduledFolders.contains(folder)) {
        qCInfo(lcFolderMan) << "Can not run etag job: Folder is already scheduled";
        return;
    }
    if (_disabledFolders.contains(folder)) {
        qCInfo(lcFolderMan) << "Can not run etag job: Folder is disabled";
        return;
    }
    if (folder->etagJob() || folder->isBusy() || !folder->canSync()) {
        qCInfo(lcFolderMan) << "Can not run etag job: Folder is busy";
        return;
    }
    // When not using push notifications, make sure polltime is reached
    if (!pushNotificationsFilesReady(folder->accountState()->account())) {
        if (folder->msecSinceLastSync() < polltime) {
            qCInfo(lcFolderMan) << "Can not run etag job: Polltime not reached";
            return;
        }
    }

    QMetaObject::invokeMethod(folder, "slotRunEtagJob", Qt::QueuedConnection);
}

void FolderMan::slotAccountRemoved(AccountState *accountState)
{
    QVector<Folder *> foldersToRemove;
    for (const auto &folder : std::as_const(_folderMap)) {
        if (folder->accountState() == accountState) {
            foldersToRemove.push_back(folder);
        }
    }
    for (const auto &folder : std::as_const(foldersToRemove)) {
        removeFolder(folder);
    }
}

void FolderMan::slotRemoveFoldersForAccount(AccountState *accountState)
{
    QVarLengthArray<Folder *, 16> foldersToRemove;
    Folder::MapIterator i(_folderMap);
    while (i.hasNext()) {
        i.next();
        Folder *folder = i.value();
        if (folder->accountState() == accountState) {
            foldersToRemove.append(folder);
        }
    }

    for (const auto &f : std::as_const(foldersToRemove)) {
        removeFolder(f);
    }
    emit folderListChanged(_folderMap);
}

void FolderMan::slotForwardFolderSyncStateChange()
{
    if (auto *f = qobject_cast<Folder *>(sender())) {
        emit folderSyncStateChange(f);
    }
}

void FolderMan::slotServerVersionChanged(const OCC::AccountPtr &account)
{
    // Pause folders if the server version is unsupported
    if (account->serverVersionUnsupported()) {
        qCWarning(lcFolderMan) << "The server version is unsupported:" << account->serverVersion()
                               << "pausing all folders on the account";

        for (auto &f : std::as_const(_folderMap)) {
            if (f->accountState()->account().data() == account) {
                f->setSyncPaused(true);
            }
        }
    }
}

void FolderMan::slotWatchedFileUnlocked(const QString &path)
{
    if (Folder *f = folderForPath(path)) {
        // Treat this equivalently to the file being reported by the file watcher
        f->slotWatchedPathChanged(path, Folder::ChangeReason::UnLock);
    }
}

void FolderMan::slotScheduleFolderByTime()
{
    for (const auto &f : std::as_const(_folderMap)) {
        // Never schedule if syncing is disabled or when we're currently
        // querying the server for etags
        if (!f->canSync() || f->etagJob()) {
            continue;
        }

        auto msecsSinceSync = f->msecSinceLastSync();

        // Possibly it's just time for a new sync run
        bool forceSyncIntervalExpired = msecsSinceSync > ConfigFile().forceSyncInterval();
        if (forceSyncIntervalExpired) {
            qCInfo(lcFolderMan) << "Scheduling folder" << f->alias()
                                << "because it has been" << msecsSinceSync.count() << "ms "
                                << "since the last sync";

            scheduleFolder(f);
            continue;
        }

        // Retry a couple of times after failure; or regularly if requested
        bool syncAgain =
            (f->consecutiveFailingSyncs() > 0 && f->consecutiveFailingSyncs() < 3)
            || f->syncEngine().isAnotherSyncNeeded() == DelayedFollowUp;
        auto syncAgainDelay = std::chrono::seconds(10); // 10s for the first retry-after-fail
        if (f->consecutiveFailingSyncs() > 1)
            syncAgainDelay = std::chrono::seconds(60); // 60s for each further attempt
        if (syncAgain && msecsSinceSync > syncAgainDelay) {
            qCInfo(lcFolderMan) << "Scheduling folder" << f->alias()
                                << ", the last" << f->consecutiveFailingSyncs() << "syncs failed"
                                << ", anotherSyncNeeded" << f->syncEngine().isAnotherSyncNeeded()
                                << ", last status:" << f->syncResult().statusString()
                                << ", time since last sync:" << msecsSinceSync.count();

            scheduleFolder(f);
            continue;
        }

        // Do we want to retry failing syncs or another-sync-needed runs more often?
    }
}

bool FolderMan::isAnySyncRunning() const
{
    if (_currentSyncFolder)
        return true;

    for (auto f : _folderMap) {
        if (f->isSyncRunning())
            return true;
    }
    return false;
}

void FolderMan::slotFolderSyncStarted()
{
    auto f = qobject_cast<Folder *>(sender());
    ASSERT(f);
    if (!f)
        return;

    qCInfo(lcFolderMan, ">========== Sync started for folder [%s] of account [%s] with remote [%s]",
        qPrintable(f->shortGuiLocalPath()),
        qPrintable(f->accountState()->account()->displayName()),
        qPrintable(f->remoteUrl().toString()));
}

/*
  * a folder indicates that its syncing is finished.
  * Start the next sync after the system had some milliseconds to breath.
  * This delay is particularly useful to avoid late file change notifications
  * (that we caused ourselves by syncing) from triggering another spurious sync.
  */
void FolderMan::slotFolderSyncFinished(const SyncResult &)
{
    auto f = qobject_cast<Folder *>(sender());
    ASSERT(f);
    if (!f)
        return;

    qCInfo(lcFolderMan, "<========== Sync finished for folder [%s] of account [%s] with remote [%s]",
        qPrintable(f->shortGuiLocalPath()),
        qPrintable(f->accountState()->account()->displayName()),
        qPrintable(f->remoteUrl().toString()));

    if (f == _currentSyncFolder) {
        _lastSyncFolder = _currentSyncFolder;
        _currentSyncFolder = nullptr;
    }
    if (!isAnySyncRunning())
        startScheduledSyncSoon();
}

Folder *FolderMan::addFolder(AccountState *accountState, const FolderDefinition &folderDefinition)
{
    // Choose a db filename
    auto definition = folderDefinition;
    definition.journalPath = definition.defaultJournalPath(accountState->account());

    if (!ensureJournalGone(definition.absoluteJournalPath())) {
        return nullptr;
    }

#ifdef Q_OS_MACOS
    // macOS sandbox: Create security-scoped bookmark data while we still
    // have access to the path (the user just selected it via QFileDialog).
    // This bookmark will be persisted to settings and resolved on next
    // app launch to regain sandbox access.
    if (definition.securityScopedBookmarkData.isEmpty()) {
        definition.securityScopedBookmarkData = Utility::createSecurityScopedBookmarkData(definition.localPath);
    }
#endif

    auto vfs = createVfsFromPlugin(folderDefinition.virtualFilesMode);
    if (!vfs) {
        qCWarning(lcFolderMan) << "Could not load plugin for mode" << folderDefinition.virtualFilesMode;
        return nullptr;
    }

    auto folder = addFolderInternal(definition, accountState, std::move(vfs));

    // Migration: The first account that's configured for a local folder shall
    // be saved in a backwards-compatible way.
    const auto folderList = FolderMan::instance()->map();
    const auto oneAccountOnly = std::none_of(folderList.cbegin(), folderList.cend(), [folder](const auto *other) {
        return other != folder && other->cleanPath() == folder->cleanPath();
    });

    folder->setSaveBackwardsCompatible(oneAccountOnly);

    if (folder) {
        folder->setSaveBackwardsCompatible(oneAccountOnly);
        folder->saveToSettings();
        emit folderSyncStateChange(folder);
        emit folderListChanged(_folderMap);
    }

#ifdef Q_OS_WIN
    _navigationPaneHelper.scheduleUpdateCloudStorageRegistry();
#endif
    return folder;
}

Folder *FolderMan::addFolderInternal(
    FolderDefinition folderDefinition,
    AccountState *accountState,
    std::unique_ptr<Vfs> vfs)
{
    auto alias = folderDefinition.alias;
    int count = 0;
    while (folderDefinition.alias.isEmpty()
        || _folderMap.contains(folderDefinition.alias)
        || _additionalBlockedFolderAliases.contains(folderDefinition.alias)) {
        // There is already a folder configured with this name and folder names need to be unique
        folderDefinition.alias = QString::number(alias.toInt() + (++count));
    }

    auto folder = new Folder(folderDefinition, accountState, std::move(vfs), this);

#ifdef Q_OS_WIN
    if (_navigationPaneHelper.showInExplorerNavigationPane() && folderDefinition.navigationPaneClsid.isNull()) {
        folder->setNavigationPaneClsid(QUuid::createUuid());
        folder->saveToSettings();
    }
#endif

    qCInfo(lcFolderMan) << "Adding folder to Folder Map " << folder << folder->alias();
    _folderMap[folder->alias()] = folder;
    if (folder->syncPaused()) {
        _disabledFolders.insert(folder);
    }

    // See matching disconnects in unloadFolder().
    connect(folder, &Folder::syncStarted, this, &FolderMan::slotFolderSyncStarted);
    connect(folder, &Folder::syncFinished, this, &FolderMan::slotFolderSyncFinished);
    connect(folder, &Folder::syncStateChange, this, &FolderMan::slotForwardFolderSyncStateChange);
    connect(folder, &Folder::syncPausedChanged, this, &FolderMan::slotFolderSyncPaused);
    connect(folder, &Folder::canSyncChanged, this, &FolderMan::slotFolderCanSyncChanged);
    connect(&folder->syncEngine().syncFileStatusTracker(), &SyncFileStatusTracker::fileStatusChanged,
        _socketApi.data(), &SocketApi::broadcastStatusPushMessage);
    connect(folder, &Folder::watchedFileChangedExternally,
        &folder->syncEngine().syncFileStatusTracker(), &SyncFileStatusTracker::slotPathTouched);

    folder->registerFolderWatcher();
    registerFolderWithSocketApi(folder);
    return folder;
}

Folder *FolderMan::folderForPath(const QString &path)
{
    QString absolutePath = QDir::cleanPath(path) + QLatin1Char('/');

    const auto folders = this->map().values();
    const auto it = std::find_if(folders.cbegin(), folders.cend(), [absolutePath](const auto *folder) {
        const QString folderPath = folder->cleanPath() + QLatin1Char('/');
        return absolutePath.startsWith(folderPath, (Utility::isWindows() || Utility::isMac()) ? Qt::CaseInsensitive : Qt::CaseSensitive);
    });

    return it != folders.cend() ? *it : nullptr;
}

void FolderMan::addFolderToSelectiveSyncList(const QString &path, const SyncJournalDb::SelectiveSyncListType list)
{
    const auto folder = folderForPath(path);
    if (!folder) {
        return;
    }

    const QString folderPath = folder->cleanPath() + QLatin1Char('/');
    const auto relPath = path.mid(folderPath.length());

    switch (list) {
    case SyncJournalDb::SelectiveSyncListType::SelectiveSyncWhiteList:
        folder->whitelistPath(relPath);
        break;
    case SyncJournalDb::SelectiveSyncListType::SelectiveSyncBlackList:
        folder->blacklistPath(relPath);
        break;
    default:
        Q_UNREACHABLE();
    }
}

void FolderMan::whitelistFolderPath(const QString &path)
{
    addFolderToSelectiveSyncList(path, SyncJournalDb::SelectiveSyncListType::SelectiveSyncWhiteList);
}

void FolderMan::blacklistFolderPath(const QString &path)
{
    addFolderToSelectiveSyncList(path, SyncJournalDb::SelectiveSyncListType::SelectiveSyncBlackList);
}

QStringList FolderMan::findFileInLocalFolders(const QString &relPath, const AccountPtr acc)
{
    QStringList re;

    // We'll be comparing against Folder::remotePath which always starts with /
    QString serverPath = relPath;
    if (!serverPath.startsWith('/'))
        serverPath.prepend('/');

    const auto mapValues = map().values();
    for (const auto folder : mapValues) {
        if (acc && folder->accountState()->account() != acc) {
            continue;
        }
        if (!serverPath.startsWith(folder->remotePathTrailingSlash()))
            continue;

        QString path = folder->cleanPath() + '/';
        path += serverPath.mid(folder->remotePathTrailingSlash().length());
        if (FileSystem::fileExists(path)) {
            re.append(path);
        }
    }
    return re;
}

void FolderMan::removeFolder(Folder *folderToRemove)
{
    if (!folderToRemove) {
        qCCritical(lcFolderMan) << "Can not remove null folder";
        return;
    }

    qCInfo(lcFolderMan) << "Removing " << folderToRemove->alias();

    const bool currentlyRunning = folderToRemove->isSyncRunning();
    if (currentlyRunning) {
        // abort the sync now
        folderToRemove->slotTerminateSync();
    }

    if (_scheduledFolders.removeAll(folderToRemove) > 0) {
        emit scheduleQueueChanged();
    }

    folderToRemove->setSyncPaused(true);
    folderToRemove->wipeForRemoval();

    // remove the folder configuration
    folderToRemove->removeFromSettings();

    // remove Desktop.ini
    Utility::removeFavLink(folderToRemove->path());

    unloadFolder(folderToRemove);
    if (currentlyRunning) {
        // We want to schedule the next folder once this is done
        connect(folderToRemove, &Folder::syncFinished,
            this, &FolderMan::slotFolderSyncFinished);
        // Let the folder delete itself when done.
        connect(folderToRemove, &Folder::syncFinished, folderToRemove, &QObject::deleteLater);
    } else {
        delete folderToRemove;
    }

#ifdef Q_OS_WIN
    _navigationPaneHelper.scheduleUpdateCloudStorageRegistry();
#endif

    emit folderListChanged(_folderMap);
}

QString FolderMan::getBackupName(QString fullPathName) const
{
    if (fullPathName.endsWith("/"))
        fullPathName.chop(1);

    if (fullPathName.isEmpty())
        return QString();

    QString newName = fullPathName + tr(" (backup)");
    QFileInfo fi(newName);
    int cnt = 2;
    do {
        if (fi.exists()) {
            newName = fullPathName + tr(" (backup %1)").arg(cnt++);
            fi.setFile(newName);
        }
    } while (fi.exists());

    return newName;
}

bool FolderMan::startFromScratch(const QString &localFolder)
{
    if (localFolder.isEmpty()) {
        return false;
    }

    QFileInfo fi(localFolder);
    QDir parentDir(fi.dir());
    QString folderName = fi.fileName();

    // Adjust for case where localFolder ends with a /
    if (fi.isDir()) {
        folderName = parentDir.dirName();
        parentDir.cdUp();
    }

    if (fi.exists()) {
        // It exists, but is empty -> just reuse it.
        if (fi.isDir() && fi.dir().count() == 0) {
            qCDebug(lcFolderMan) << "startFromScratch: Directory is empty!";
            return true;
        }
        // Disconnect the socket api from the database to avoid that locking of the
        // db file does not allow to move this dir.
        Folder *f = folderForPath(localFolder);
        if (f) {
            if (localFolder.startsWith(f->path())) {
                _socketApi->slotUnregisterPath(f->alias());
            }
            f->journalDb()->close();
            f->slotTerminateSync(); // Normally it should not be running, but viel hilft viel
        }

        // Make a backup of the folder/file.
        QString newName = getBackupName(parentDir.absoluteFilePath(folderName));
        QString renameError;
        if (!FileSystem::rename(fi.absoluteFilePath(), newName, &renameError)) {
            qCWarning(lcFolderMan) << "startFromScratch: Could not rename" << fi.absoluteFilePath()
                                   << "to" << newName << "error:" << renameError;
            return false;
        }
    }

    if (!parentDir.mkdir(fi.absoluteFilePath())) {
        qCWarning(lcFolderMan) << "startFromScratch: Could not mkdir" << fi.absoluteFilePath();
        return false;
    }

    return true;
}

void FolderMan::slotWipeFolderForAccount(AccountState *accountState)
{
    QVarLengthArray<Folder *, 16> foldersToRemove;
    Folder::MapIterator i(_folderMap);
    while (i.hasNext()) {
        i.next();
        Folder *folder = i.value();
        if (folder->accountState() == accountState) {
            foldersToRemove.append(folder);
        }
    }

    bool success = false;
    for (const auto &f : std::as_const(foldersToRemove)) {
        if (!f) {
            qCCritical(lcFolderMan) << "Can not remove null folder";
            return;
        }

        qCInfo(lcFolderMan) << "Removing " << f->alias();

        const bool currentlyRunning = (_currentSyncFolder == f);
        if (currentlyRunning) {
            // abort the sync now
            _currentSyncFolder->slotTerminateSync();
        }

        if (_scheduledFolders.removeAll(f) > 0) {
            emit scheduleQueueChanged();
        }

        // wipe database
        f->wipeForRemoval();

        // wipe data
        QDir userFolder(f->path());
        if (userFolder.exists()) {
            success = FileSystem::removeRecursively(f->path());
            if (!success) {
                qCWarning(lcFolderMan) << "Failed to remove existing folder " << f->path();
            } else {
                qCInfo(lcFolderMan) << "wipe: Removed  file " << f->path();
            }


        } else {
            success = true;
            qCWarning(lcFolderMan) << "folder does not exist, can not remove.";
        }

        f->setSyncPaused(true);

        // remove the folder configuration
        f->removeFromSettings();

        unloadFolder(f);
        if (currentlyRunning) {
            delete f;
        }

#ifdef Q_OS_WIN
        _navigationPaneHelper.scheduleUpdateCloudStorageRegistry();
#endif
    }

    emit folderListChanged(_folderMap);
    emit wipeDone(accountState, success);
}

void FolderMan::setDirtyProxy()
{
    const auto folderMapValues = _folderMap.values();
    for (const auto folder : folderMapValues) {
        if (folder 
            && folder->accountState() 
            && folder->accountState()->account()
            && folder->accountState()->account()->networkAccessManager()) {
            // Need to do this so we do not use the old determined system proxy
            const auto proxy = QNetworkProxy(QNetworkProxy::DefaultProxy);
            folder->accountState()->account()->setProxyType(proxy.type());
        }
    }
}

void FolderMan::setDirtyNetworkLimits()
{
    const auto folderMapValues = _folderMap.values();
    for (auto folder : folderMapValues) {
        // set only in busy folders. Otherwise they read the config anyway.
        if (folder && folder->isBusy()) {
            folder->setDirtyNetworkLimits();
        }
    }
}

void FolderMan::setDirtyNetworkLimits(const AccountPtr &account) const
{
    const auto folderMapValues = _folderMap.values();
    for (const auto folder : folderMapValues) {
        // set only in busy folders. Otherwise they read the config anyway.
        if (folder && folder->isBusy() && folder->accountState()->account() == account) {
            folder->setDirtyNetworkLimits();
        }
    }
}

void FolderMan::leaveShare(const QString &localFile)
{
    const auto localFileNoTrailingSlash = localFile.endsWith('/') ? localFile.chopped(1) : localFile;
    if (const auto folder = FolderMan::instance()->folderForPath(localFileNoTrailingSlash)) {
        const auto filePathRelative = Utility::noLeadingSlashPath(QString(localFileNoTrailingSlash).remove(folder->path()));

        SyncJournalFileRecord rec;
        if (folder->journalDb()->getFileRecord(filePathRelative, &rec)
            && rec.isValid() && rec.isE2eEncrypted()) {

            if (_removeE2eeShareJob) {
                _removeE2eeShareJob->deleteLater();
            }

            _removeE2eeShareJob = new UpdateE2eeFolderUsersMetadataJob(folder->accountState()->account(),
                                                                                 folder->journalDb(),
                                                                                 folder->remotePath(),
                                                                                 UpdateE2eeFolderUsersMetadataJob::Remove,
                                                                                 folder->remotePathTrailingSlash() + filePathRelative,
                                                                                 folder->accountState()->account()->davUser());
            _removeE2eeShareJob->setParent(this);
            _removeE2eeShareJob->start(true);
            connect(_removeE2eeShareJob, &UpdateE2eeFolderUsersMetadataJob::finished, this, [localFileNoTrailingSlash, this](int code, const QString &message) {   
                if (code != 200) {
                    qCWarning(lcFolderMan) << "Could not remove share from E2EE folder's metadata!" << code << message;
                    return;
                }
                slotLeaveShare(localFileNoTrailingSlash, _removeE2eeShareJob->folderToken());
            });

            return;
        }
        slotLeaveShare(localFileNoTrailingSlash);
    }
}

void FolderMan::slotLeaveShare(const QString &localFile, const QByteArray &folderToken)
{
    const auto folder = FolderMan::instance()->folderForPath(localFile);

    if (!folder) {
        qCWarning(lcFolderMan) << "Could not find a folder for localFile:" << localFile;
        return;
    }

    const auto filePathRelative = QString(localFile).remove(folder->path());
    const auto leaveShareJob = new SimpleApiJob(folder->accountState()->account(), folder->accountState()->account()->davPath() + filePathRelative);
    leaveShareJob->setVerb(SimpleApiJob::Verb::Delete);
    leaveShareJob->addRawHeader("e2e-token", folderToken);
    connect(leaveShareJob, &SimpleApiJob::resultReceived, this, [this, folder, localFile](int statusCode) {
        qCDebug(lcFolderMan) << "slotLeaveShare callback statusCode" << statusCode;
        Q_UNUSED(statusCode);
        if (_removeE2eeShareJob) {
            _removeE2eeShareJob->unlockFolder(EncryptedFolderMetadataHandler::UnlockFolderWithResult::Success);
            connect(_removeE2eeShareJob.data(), &UpdateE2eeFolderUsersMetadataJob::folderUnlocked, this, [this, folder] {
                scheduleFolder(folder);
            });
            return;
        }
        scheduleFolder(folder);
    });
    leaveShareJob->start();
}

void FolderMan::trayOverallStatus(const QList<Folder *> &folders,
                                  SyncResult::Status *status,
                                  bool *unresolvedConflicts,
                                  ProgressInfo **const overallProgressInfo)
{
    *status = SyncResult::Undefined;
    *unresolvedConflicts = false;

    const auto cnt = folders.count();

    // if one folder: show the state of the one folder along with the sync status.
    // if more folders:
    // if one of them has an error -> show error
    // if one is paused, but others ok, show ok
    // do not show "problem" in the tray
    // and do not show sync status
    //
    if (cnt == 1) {
        const auto folder = folders.at(0);
        if (folder) {
            auto syncResult = folder->syncResult();
            if (folder->syncPaused()) {
                *status = SyncResult::Paused;
            } else {
                SyncResult::Status syncStatus = syncResult.status();
                switch (syncStatus) {
                case SyncResult::Undefined:
                    *status = SyncResult::Error;
                    break;
                case SyncResult::Problem: // don't show the problem icon in tray.
                    *status = SyncResult::Success;
                    break;
                default:
                    *status = syncStatus;
                    break;
                }
            }
            *unresolvedConflicts = syncResult.hasUnresolvedConflicts();
            *overallProgressInfo = folder->syncEngine().progressInfo();
        }
    } else {
        auto errorsSeen = false;
        auto goodSeen = false;
        auto abortOrPausedSeen = false;
        auto runSeen = false;
        auto various = false;

        for (const auto folder : std::as_const(folders)) {
            // We've already seen an error, worst case met.
            // No need to check the remaining folders.
            if (errorsSeen) {
                break;
            }

            const auto folderResult = folder->syncResult();

            if (folder->syncPaused()) {
                abortOrPausedSeen = true;
            } else {
                const auto syncStatus = folderResult.status();

                switch (syncStatus) {
                case SyncResult::Undefined:
                case SyncResult::NotYetStarted:
                    various = true;
                    break;
                case SyncResult::SyncPrepare:
                case SyncResult::SyncRunning:
                    runSeen = true;
                    break;
                case SyncResult::Problem: // don't show the problem icon in tray.
                case SyncResult::Success:
                    goodSeen = true;
                    break;
                case SyncResult::Error:
                case SyncResult::SetupError:
                    errorsSeen = true;
                    break;
                case SyncResult::SyncAbortRequested:
                case SyncResult::Paused:
                    abortOrPausedSeen = true;
                    // no default case on purpose, check compiler warnings
                }
            }

            if (folderResult.hasUnresolvedConflicts()) {
                *unresolvedConflicts = true;
            }
        }

        if (errorsSeen) {
            *status = SyncResult::Error;
        } else if (abortOrPausedSeen) {
            // only if all folders are paused
            *status = SyncResult::Paused;
        } else if (runSeen) {
            *status = SyncResult::SyncRunning;
        } else if (goodSeen) {
            *status = SyncResult::Success;
        } else if (various) {
            *status = SyncResult::Undefined;
        }
    }
}

QString FolderMan::trayTooltipStatusString(SyncResult::Status syncStatus, bool hasUnresolvedConflicts, bool paused, ProgressInfo *const progress)
{
    QString folderMessage;
    switch (syncStatus) {
    case SyncResult::Undefined:
        folderMessage = tr("Undefined state.");
        break;
    case SyncResult::NotYetStarted:
        folderMessage = tr("Waiting to start syncing.");
        break;
    case SyncResult::SyncPrepare:
        folderMessage = tr("Preparing for sync.");
        break;
    case SyncResult::SyncRunning:
        if (progress && progress->status() == ProgressInfo::Propagation) {
            const auto estimatedEta = progress->totalProgress().estimatedEta;
            if (progress->totalSize() == 0) {
                qint64 currentFile = progress->currentFile();
                qint64 totalFileCount = qMax(progress->totalFiles(), currentFile);
                if (progress->trustEta()) {
                    if (estimatedEta == 0) {
                        folderMessage = tr("Syncing %1 of %2 (A few seconds left)").arg(currentFile).arg(totalFileCount);
                    } else {
                        folderMessage =
                            tr("Syncing %1 of %2 (%3 left)").arg(currentFile).arg(totalFileCount).arg(Utility::durationToDescriptiveString1(estimatedEta));
                    }
                } else {
                    folderMessage = tr("Syncing %1 of %2").arg(currentFile).arg(totalFileCount);
                }
            } else {
                QString totalSizeStr = Utility::octetsToString(progress->totalSize());
                if (progress->trustEta()) {
                    if (estimatedEta == 0) {
                        folderMessage = tr("Syncing %1 (A few seconds left)").arg(totalSizeStr);
                    } else {
                        folderMessage = tr("Syncing %1 (%2 left)").arg(totalSizeStr, Utility::durationToDescriptiveString1(estimatedEta));
                    }
                } else {
                    folderMessage = tr("Syncing %1").arg(totalSizeStr);
                }
            }
            break;
        }
        folderMessage = tr("Sync is running.");
        break;
    case SyncResult::Success:
    case SyncResult::Problem:
        if (hasUnresolvedConflicts) {
            folderMessage = tr("Sync finished with unresolved conflicts.");
        } else {
            folderMessage = tr("Last sync was successful.");
        }
        break;
    case SyncResult::Error:
        break;
    case SyncResult::SetupError:
        folderMessage = tr("Setup error.");
        break;
    case SyncResult::SyncAbortRequested:
        folderMessage = tr("Sync request was cancelled.");
        break;
    case SyncResult::Paused:
        folderMessage = tr("Sync is paused.");
        break;
        // no default case on purpose, check compiler warnings
    }
    if (paused) {
        // sync is disabled.
        folderMessage = tr("%1 (Sync is paused)").arg(folderMessage);
    }
    return folderMessage;
}

static QString checkPathValidityRecursive(const QString &path)
{
    if (path.isEmpty()) {
        return FolderMan::tr("Please choose a different location. The selected folder isn't valid.");
    }

#ifdef Q_OS_WIN
    Utility::NtfsPermissionLookupRAII ntfs_perm;
#endif
    const QFileInfo selFile(path);
    if (numberOfSyncJournals(selFile.filePath()) != 0) {
        return FolderMan::tr("Please choose a different location. %1 is already being used as a sync folder.").arg(QDir::toNativeSeparators(selFile.filePath()));
    }

    if (!FileSystem::fileExists(path)) {
        QString parentPath = selFile.dir().path();
        if (parentPath != path) {
            return checkPathValidityRecursive(parentPath);
        }

        return FolderMan::tr("Please choose a different location. The path %1 doesn't exist.").arg(QDir::toNativeSeparators(selFile.filePath()));
    }

    if (!FileSystem::isDir(path)) {
        return FolderMan::tr("Please choose a different location. The path %1 isn't a folder.").arg(QDir::toNativeSeparators(selFile.filePath()));
    }

    #ifdef Q_OS_WIN
    if (!FileSystem::isWritable(path)) {
        // isWritable() doesn't cover all NTFS permissions
        // try creating and removing a test file, and make sure it is excluded from sync
        if (!Utility::canCreateFileInPath(path)) {
            return FolderMan::tr("Please choose a different location. You don't have enough permissions to write to %1.", "folder location").arg(QDir::toNativeSeparators(selFile.filePath()));
        }
    }
    #else
    if (!FileSystem::isWritable(path)) {
        return FolderMan::tr("Please choose a different location. You don't have enough permissions to write to %1.", "folder location").arg(QDir::toNativeSeparators(selFile.filePath()));
    }
    #endif
    return {};
}

// QFileInfo::canonicalPath returns an empty string if the file does not exist.
// This function also works with files that does not exist and resolve the symlinks in the
// parent directories.
static QString canonicalPath(const QString &path)
{
    QFileInfo selFile(path);
    if (!FileSystem::fileExists(path)) {
        const auto parentPath = selFile.dir().path();

        // It's possible for the parentPath to match the path
        // (possibly we've arrived at a non-existent drive root on Windows)
        // and recursing would be fatal.
        if (parentPath == path) {
            return path;
        }

        return canonicalPath(parentPath) + '/' + selFile.fileName();
    }
    return selFile.canonicalFilePath();
}

QPair<FolderMan::PathValidityResult, QString> FolderMan::checkPathValidityForNewFolder(const QString &path, const QUrl &serverUrl) const
{
    QPair<FolderMan::PathValidityResult, QString> result;

    const auto recursiveValidity = checkPathValidityRecursive(path);
    if (!recursiveValidity.isEmpty()) {
        qCDebug(lcFolderMan) << path << recursiveValidity;
        result.first = FolderMan::PathValidityResult::ErrorRecursiveValidity;
        result.second = recursiveValidity;
        return result;
    }

    // check if the local directory isn't used yet in another ownCloud sync
    Qt::CaseSensitivity cs = Qt::CaseSensitive;
    if (Utility::fsCasePreserving()) {
        cs = Qt::CaseInsensitive;
    }

    const QString userDir = QDir::cleanPath(canonicalPath(path)) + '/';
    for (auto i = _folderMap.constBegin(); i != _folderMap.constEnd(); ++i) {
        auto *f = static_cast<Folder *>(i.value());
        QString folderDir = QDir::cleanPath(canonicalPath(f->path())) + '/';

        bool differentPaths = QString::compare(folderDir, userDir, cs) != 0;
        if (differentPaths && folderDir.startsWith(userDir, cs)) {
            result.first = FolderMan::PathValidityResult::ErrorContainsFolder;
            result.second = tr("Please choose a different location. %1 is already being used as a sync folder.")
                                .arg(QDir::toNativeSeparators(path));
            return result;
        }

        if (differentPaths && userDir.startsWith(folderDir, cs)) {
            result.first = FolderMan::PathValidityResult::ErrorContainedInFolder;
            result.second = tr("Please choose a different location. %1 is already contained in a folder used as a sync folder.")
                                .arg(QDir::toNativeSeparators(path));
            return result;
        }

        // if both paths are equal, the server url needs to be different
        // otherwise it would mean that a new connection from the same local folder
        // to the same account is added which is not wanted. The account must differ.
        if (serverUrl.isValid() && !differentPaths) {
            QUrl folderUrl = f->accountState()->account()->url();
            QString user = f->accountState()->account()->credentials()->user();
            folderUrl.setUserName(user);

            if (serverUrl == folderUrl) {
                result.first = FolderMan::PathValidityResult::ErrorNonEmptyFolder;
                result.second = tr("Please choose a different location. %1 is already being used as a sync folder for %2.", "folder location, server url")
                                    .arg(QDir::toNativeSeparators(path),
                                         serverUrl.toString());
                return result;
            }
        }
    }

    return result;
}

QString FolderMan::findGoodPathForNewSyncFolder(const QString &basePath, const QUrl &serverUrl, GoodPathStrategy allowExisting) const
{
    QString folder = basePath;

    // If the parent folder is a sync folder or contained in one, we can't
    // possibly find a valid sync folder inside it.
    // Example: Someone syncs their home directory. Then ~/foobar is not
    // going to be an acceptable sync folder path for any value of foobar.
    QString parentFolder = QFileInfo(folder).dir().canonicalPath();
    if (FolderMan::instance()->folderForPath(parentFolder)) {
        // Any path with that parent is going to be unacceptable,
        // so just keep it as-is.
        return basePath;
    }

    int attempt = 1;
    forever {
        const auto isGood = FolderMan::instance()->checkPathValidityForNewFolder(folder, serverUrl).second.isEmpty() &&
            (allowExisting == GoodPathStrategy::AllowOverrideExistingPath || !FileSystem::fileExists(folder));
        if (isGood) {
            break;
        }

        // Count attempts and give up eventually
        attempt++;
        if (attempt > 100) {
            return basePath;
        }

        folder = basePath + QString::number(attempt);
    }

    return folder;
}

bool FolderMan::ignoreHiddenFiles() const
{
    if (_folderMap.empty()) {
        // Currently no folders in the manager -> return default
        return false;
    }
    // Since the hiddenFiles settings is the same for all folders, just return the settings of the first folder
    return _folderMap.begin().value()->ignoreHiddenFiles();
}

void FolderMan::setIgnoreHiddenFiles(bool ignore)
{
    // Note that the setting will revert to 'true' if all folders
    // are deleted...
    for (const auto folder : std::as_const(_folderMap)) {
        folder->setIgnoreHiddenFiles(ignore);
        folder->saveToSettings();
    }
}

QQueue<Folder *> FolderMan::scheduleQueue() const
{
    return _scheduledFolders;
}

Folder *FolderMan::currentSyncFolder() const
{
    return _currentSyncFolder;
}

void FolderMan::restartApplication()
{
    if (Utility::isLinux()) {
        // restart:
        qCInfo(lcFolderMan) << "Restarting application NOW, PID" << qApp->applicationPid() << "is ending.";
        qApp->quit();
        QStringList args = qApp->arguments();
        QString prg = args.takeFirst();

        QProcess::startDetached(prg, args);
    } else {
        qCDebug(lcFolderMan) << "On this platform we do not restart.";
    }
}

void FolderMan::slotSetupPushNotifications(const Folder::Map &folderMap)
{
    for (auto folder : folderMap) {
        const auto account = folder->accountState()->account();

        // See if the account already provides the PushNotifications object and if yes connect to it.
        // If we can't connect at this point, the signals will be connected in slotPushNotificationsReady()
        // after the PushNotification object emitted the ready signal
        slotConnectToPushNotifications(account);
        connect(account.data(), &Account::pushNotificationsReady, this, &FolderMan::slotConnectToPushNotifications, Qt::UniqueConnection);
    }
}

void FolderMan::slotProcessFilesPushNotification(Account *account)
{
    qCDebug(lcFolderMan) << "received notify_file push notification account=" << account->displayName();

    for (auto folder : std::as_const(_folderMap)) {
        // Just run on the folders that belong to this account
        if (folder->accountState()->account() != account) {
            continue;
        }

        qCInfo(lcFolderMan).nospace() << "scheduling sync folder=" << folder->alias() << " account=" << account->displayName() << " reason=notify_file";
        scheduleFolder(folder);
    }
}

void FolderMan::slotProcessFileIdsPushNotification(Account *account, const QList<qint64> &fileIds)
{
    qCDebug(lcFolderMan).nospace() << "received notify_file_id push notification account=" << account->displayName() << " fileIds=" << fileIds;

    for (auto folder : std::as_const(_folderMap)) {
        // Just run on the folders that belong to this account
        if (folder->accountState()->account() != account) {
            continue;
        }

        if (!folder->hasFileIds(fileIds)) {
            qCDebug(lcFolderMan).nospace() << "no matching file ids, ignoring folder=" << folder->alias() << " account=" << account->displayName();
            continue;
        }

        qCInfo(lcFolderMan).nospace() << "scheduling sync folder=" << folder->alias() << " account=" << account->displayName() << " reason=notify_file_id";
        scheduleFolder(folder);
    }
}

void FolderMan::slotConnectToPushNotifications(const AccountPtr &account)
{
    const auto pushNotifications = account->pushNotifications();

    if (pushNotificationsFilesReady(account)) {
        qCInfo(lcFolderMan) << "Push notifications ready";
        connect(pushNotifications, &PushNotifications::filesChanged, this, &FolderMan::slotProcessFilesPushNotification, Qt::UniqueConnection);
        connect(pushNotifications, &PushNotifications::fileIdsChanged, this, &FolderMan::slotProcessFileIdsPushNotification, Qt::UniqueConnection);
    }
}

bool FolderMan::checkVfsAvailability(const QString &path, Vfs::Mode mode) const
{
    return unsupportedConfiguration(path) && Vfs::checkAvailability(path, mode);
}

Result<void, QString> FolderMan::unsupportedConfiguration(const QString &path) const
{
    if (numberOfSyncJournals(path) > 1) {
        return tr("The folder %1 is linked to multiple accounts.\n"
                  "This setup can cause data loss and it is no longer supported.\n"
                  "To resolve this issue: please remove %1 from one of the accounts and create a new sync folder.\n"
                  "For advanced users: this issue might be related to multiple sync database files found in one folder. Please check %1 for outdated and unused .sync_*.db files and remove them.")
            .arg(path);
    }
    return {};
}

} // namespace OCC
