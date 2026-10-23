/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QHeaderView>
#include <QScopeGuard>
#include <QTreeView>
#include <QtTest>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include "configfile.h"
#include "folderman.h"
#include "folderstatusdelegate.h"
#include "folderstatusmodel.h"
#include "owncloudgui.h"
#include "progressdispatcher.h"
#include "syncengine.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

using namespace OCC;
using namespace Qt::StringLiterals;

class TestDiscoveryProgress : public QObject
{
    Q_OBJECT
    QTemporaryDir _configuration;
#ifdef Q_OS_WIN
    HKEY _userRegistry = nullptr;
    HKEY _testRegistry = nullptr;
    QString _testRegistryPath;
    bool _registryOverridden = false;
#endif

private Q_SLOTS:
    void initTestCase()
    {
#ifdef Q_OS_WIN
        // FolderMan schedules Explorer integration updates even for temporary accounts.
        _testRegistryPath = u"Software\\NextcloudDiscoveryProgressTest-%1"_s.arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        QCOMPARE(RegOpenCurrentUser(KEY_ALL_ACCESS, &_userRegistry), ERROR_SUCCESS);
        QCOMPARE(RegCreateKeyExW(_userRegistry,
                                 reinterpret_cast<const wchar_t *>(_testRegistryPath.utf16()),
                                 0,
                                 nullptr,
                                 REG_OPTION_VOLATILE,
                                 KEY_ALL_ACCESS,
                                 nullptr,
                                 &_testRegistry,
                                 nullptr),
                 ERROR_SUCCESS);
        QCOMPARE(RegOverridePredefKey(HKEY_CURRENT_USER, _testRegistry), ERROR_SUCCESS);
        _registryOverridden = true;
#endif
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(_configuration.isValid());
        QVERIFY(ConfigFile::setConfDir(_configuration.path()));
        FolderMan::instance()->setSyncEnabled(false);
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }

    void cleanupTestCase()
    {
        FolderMan::resetInstance();
#ifdef Q_OS_WIN
        if (_registryOverridden) {
            QCOMPARE(RegOverridePredefKey(HKEY_CURRENT_USER, nullptr), ERROR_SUCCESS);
        }
        if (_testRegistry) {
            QCOMPARE(RegCloseKey(_testRegistry), ERROR_SUCCESS);
        }
        if (_userRegistry) {
            QCOMPARE(RegDeleteTreeW(_userRegistry, reinterpret_cast<const wchar_t *>(_testRegistryPath.utf16())), ERROR_SUCCESS);
            QCOMPARE(RegCloseKey(_userRegistry), ERROR_SUCCESS);
        }
#endif
    }

    void countsExaminedItemsAndResets()
    {
        ProgressInfo progress;
        SyncFileItem item;
        item._type = ItemTypeDirectory;
        QVERIFY(!progress.recordDiscoveredItem(item));
        progress._status = ProgressInfo::Discovery;
        QVERIFY(progress.recordDiscoveredItem(item));
        item._type = ItemTypeFile;
        static_cast<void>(progress.recordDiscoveredItem(item));
        item._type = ItemTypeVirtualFile;
        static_cast<void>(progress.recordDiscoveredItem(item));
        QCOMPARE(progress.discoveredFolders(), 1);
        QCOMPARE(progress.discoveredFiles(), 2);
        QCOMPARE(progress.totalFiles(), 0);
        QCOMPARE(progress.preparationStatusString(), u"Checking for changes — folders: 1, files: 2"_s);
        progress._status = ProgressInfo::Propagation;
        QVERIFY(!progress.recordDiscoveredItem(item));
        QVERIFY(progress.preparationStatusString().isEmpty());
        progress._status = ProgressInfo::Done;
        QVERIFY(progress.preparationStatusString().isEmpty());
        progress.reset();
        QCOMPARE(progress.discoveredFolders(), 0);
        QCOMPARE(progress.discoveredFiles(), 0);
        QCOMPARE(progress.preparationStatusString(), u"Starting sync…"_s);
        progress._status = ProgressInfo::Discovery;
        QCOMPARE(progress.preparationStatusString(), u"Checking for changes…"_s);
        QVERIFY(progress.recordDiscoveredItem(item));
        progress._status = ProgressInfo::Reconcile;
        QCOMPARE(progress.preparationStatusString(), u"Comparing local and server changes…"_s);
    }

    void ignoresUnknownItemTypes()
    {
        ProgressInfo progress;
        progress._status = ProgressInfo::Discovery;
        SyncFileItem item;
        QVERIFY(!progress.recordDiscoveredItem(item));
        QCOMPARE(progress.discoveredFolders(), 0);
        QCOMPARE(progress.discoveredFiles(), 0);
    }

    void countsExcludedDirectories_data()
    {
        QTest::addColumn<bool>("local");
        QTest::newRow("local") << true;
        QTest::newRow("remote") << false;
    }

    void countsExcludedDirectories()
    {
        QFETCH(bool, local);
        FakeFolder folder{FileInfo{}};
        folder.syncEngine().setIgnoreHiddenFiles(true);
        auto &modifier = local ? static_cast<FileModifier &>(folder.localModifier()) : static_cast<FileModifier &>(folder.remoteModifier());
        modifier.mkdir(u".hidden-folder"_s);
        modifier.insert(u".hidden-file"_s);
        modifier.insert(u".hidden-folder/not-visited"_s);
        QVERIFY(folder.syncOnce());
        const auto progress = folder.syncEngine().progressInfo();
        QCOMPARE(progress->discoveredFolders(), 1);
        QCOMPARE(progress->discoveredFiles(), 1);
        QCOMPARE(progress->totalFiles(), 0);
        QCOMPARE(folder.currentLocalState().find(u".hidden-folder/not-visited"_s) != nullptr, local);
        QCOMPARE(folder.currentRemoteState().find(u".hidden-folder/not-visited"_s) != nullptr, !local);
    }

    void reopeningSettingsShowsCurrentPreparationState()
    {
        FakeFolder fakeFolder{FileInfo{}};
        const auto accountState = new FakeAccountState(fakeFolder.account());
        QTemporaryDir previewFolder;
        QVERIFY(previewFolder.isValid());
        auto definition = folderDefinition(previewFolder.path());
        definition.alias = u"progress-reopen"_s;
        definition.targetPath.clear();
        const auto folder = FolderMan::instance()->addFolder(accountState, definition);
        QVERIFY(folder);
        const auto cleanup = qScopeGuard([folder] {
            FolderMan::instance()->removeFolder(folder);
        });
        folder->setSyncState(SyncResult::SyncPrepare);
        const auto progress = folder->syncEngine().progressInfo();
        progress->_status = ProgressInfo::Discovery;
        SyncFileItem item;
        item._type = ItemTypeDirectory;
        static_cast<void>(progress->recordDiscoveredItem(item));

        const auto checkReopenedModel = [&](const QString &expected) {
            FolderStatusModel model;
            model.setAccountState(accountState);
            const auto index = model.index(0);
            QVERIFY(index.isValid());
            QCOMPARE(model.data(index, FolderStatusDelegate::SyncProgressOverallString).toString(), expected);
            QCOMPARE(model.data(index, FolderStatusDelegate::SyncProgressOverallPercent).toInt(), expected.isEmpty() ? 0 : -1);
        };
        checkReopenedModel(u"Checking for changes — folders: 1, files: 0"_s);
        progress->_status = ProgressInfo::Reconcile;
        checkReopenedModel(u"Comparing local and server changes…"_s);
        accountState->setStateForTesting(AccountState::Disconnected);
        checkReopenedModel(u"Waiting for a connection…"_s);
        accountState->setStateForTesting(AccountState::NetworkError);
        checkReopenedModel(u"Connection interrupted — waiting to retry…"_s);
        accountState->setStateForTesting(AccountState::SignedOut);
        checkReopenedModel({});
        accountState->setStateForTesting(AccountState::Disconnected);
        folder->setSyncPaused(true);
        checkReopenedModel({});
    }

    void throttlesNotificationsWithoutDroppingCounts()
    {
        ProgressInfo progress;
        progress._status = ProgressInfo::Discovery;
        SyncFileItem item;
        item._type = ItemTypeFile;
        QVERIFY(progress.recordDiscoveredItem(item));
        QVERIFY(!progress.recordDiscoveredItem(item));
        QCOMPARE(progress.discoveredFiles(), 2);
        QTest::qWait(ProgressInfo::DiscoveryProgressIntervalMs + 20);
        QVERIFY(progress.recordDiscoveredItem(item));
        QCOMPARE(progress.discoveredFiles(), 3);
    }

    void trayReceivesPreparationProgress()
    {
        FakeFolder fakeFolder{FileInfo{}, {}, {}, false};
        const auto accountManager = AccountManager::instance();
        QVERIFY(accountManager->accounts().isEmpty());
        ownCloudGui gui;
        const auto tray = Systray::instance();
        const QSignalBlocker traySignals(tray);
        const auto accountState = accountManager->addAccount(fakeFolder.account());
        const auto accountCleanup = qScopeGuard([accountManager, accountState] {
            accountManager->removeAccountState(accountState);
        });
        QVERIFY(QMetaObject::invokeMethod(accountState,
                                          "slotConnectionValidatorResult",
                                          Qt::DirectConnection,
                                          Q_ARG(OCC::ConnectionValidator::Status, ConnectionValidator::Connected),
                                          Q_ARG(QStringList, QStringList{})));
        QVERIFY(accountState->isConnected());

        QTemporaryDir localFolder;
        QVERIFY(localFolder.isValid());
        auto definition = folderDefinition(localFolder.path());
        definition.alias = u"progress-tray"_s;
        definition.targetPath.clear();
        const auto folder = FolderMan::instance()->addFolder(accountState, definition);
        QVERIFY(folder);
        const auto folderCleanup = qScopeGuard([folder] {
            FolderMan::instance()->removeFolder(folder);
        });
        folder->setSyncState(SyncResult::SyncPrepare);
        const auto progress = folder->syncEngine().progressInfo();
        progress->_status = ProgressInfo::Starting;
        gui.slotComputeOverallSyncStatus();
        QVERIFY(tray->toolTip().contains(u"Starting sync…"_s));

        const auto publishAndCheck = [&] {
            QVERIFY(QMetaObject::invokeMethod(folder, "slotTransmissionProgress", Qt::DirectConnection, Q_ARG(OCC::ProgressInfo, *progress)));
            QVERIFY2(tray->toolTip().contains(progress->preparationStatusString()), qPrintable(tray->toolTip()));
        };
        progress->_status = ProgressInfo::Discovery;
        publishAndCheck();
        SyncFileItem item;
        item._type = ItemTypeDirectory;
        static_cast<void>(progress->recordDiscoveredItem(item));
        item._type = ItemTypeFile;
        static_cast<void>(progress->recordDiscoveredItem(item));
        publishAndCheck();
        static_cast<void>(progress->recordDiscoveredItem(item));
        publishAndCheck();
        progress->_status = ProgressInfo::Reconcile;
        publishAndCheck();
        tray->hide();
    }

    void enginePublishesFinalCountsAndResetsBetweenRuns()
    {
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        folder.remoteModifier().insert(u"A/new-file"_s);
        auto files = qint64{0};
        auto directories = qint64{0};
        auto sawReconcile = false;
        connect(&folder.syncEngine(), &SyncEngine::itemDiscovered, this, [&](const SyncFileItemPtr &item) {
            item->isDirectory() ? ++directories : ++files;
        });
        connect(&folder.syncEngine(), &SyncEngine::transmissionProgress, this, [&](const ProgressInfo &progress) {
            if (progress.status() == ProgressInfo::Reconcile) {
                sawReconcile = true;
                QCOMPARE(progress.discoveredFiles(), files);
                QCOMPARE(progress.discoveredFolders(), directories);
            }
        });
        QVERIFY(folder.syncOnce());
        QVERIFY(sawReconcile);
        QVERIFY(files > 0);
        QVERIFY(directories > 0);
        files = 0;
        directories = 0;
        sawReconcile = false;
        QVERIFY(folder.syncOnce());
        QVERIFY(sawReconcile);
    }

    void abortFromProgressDoesNotPropagateOrKeepCounting()
    {
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        folder.remoteModifier().insert(u"A/new-file"_s);
        auto aborted = false;
        auto propagated = false;
        connect(&folder.syncEngine(), &SyncEngine::aboutToPropagate, this, [&] {
            propagated = true;
        });
        const auto connection = connect(&folder.syncEngine(), &SyncEngine::transmissionProgress, this, [&](const ProgressInfo &progress) {
            if (!aborted && progress.status() == ProgressInfo::Discovery && progress.discoveredFiles() + progress.discoveredFolders() > 0) {
                aborted = true;
                folder.syncEngine().abort();
            }
        });
        QVERIFY(!folder.syncOnce());
        QVERIFY(aborted);
        QVERIFY(!propagated);
        const auto progress = folder.syncEngine().progressInfo();
        const auto examined = progress->discoveredFiles() + progress->discoveredFolders();
        QCoreApplication::processEvents();
        QCOMPARE(progress->discoveredFiles() + progress->discoveredFolders(), examined);
        disconnect(connection);
        QVERIFY(folder.syncOnce());
    }

    void settingsShowCountsAndRealWaitingStates()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        const auto accountState = new FakeAccountState(fakeFolder.account());
        QTemporaryDir previewFolder;
        QVERIFY(previewFolder.isValid());
        auto definition = folderDefinition(previewFolder.path());
        definition.alias = u"progress-preview"_s;
        definition.targetPath.clear();
        definition.journalPath = u".sync_journal.db"_s;
        const auto folderOwner = std::make_unique<Folder>(definition, accountState, createVfsFromPlugin(Vfs::Off));
        const auto folder = folderOwner.get();
        QTreeView view;
        view.setFont(QFont(u"Sans Serif"_s, 10));
        FolderStatusModel model(&view);
        model.setAccountState(accountState);
        FolderStatusModel::SubFolderInfo info;
        info._folder = folder;
        info._name = definition.alias;
        info._path = u"/"_s;
        info._pathIdx = {0};
        model._folders.append(info);
        connect(folder, &Folder::progressInfo, &model, &FolderStatusModel::slotSetProgress);
        view.setModel(&model);
        const auto delegate = new FolderStatusDelegate;
        delegate->setParent(&view);
        view.setItemDelegate(delegate);
        view.header()->hide();
        view.setRootIsDecorated(false);
        view.resize(720, 180);
        view.show();
        QCoreApplication::processEvents();
        const auto index = model.index(0);
        QVERIFY(index.isValid());
        folder->setSyncState(SyncResult::SyncPrepare);
        const auto progress = folder->syncEngine().progressInfo();
        progress->_status = ProgressInfo::Done;
        QVERIFY(QMetaObject::invokeMethod(&model, "slotFolderSyncStateChange", Qt::DirectConnection, Q_ARG(OCC::Folder *, folder)));
        QCOMPARE(model.data(index, FolderStatusDelegate::SyncProgressOverallString).toString(), u"Starting sync…"_s);
        QCOMPARE(FolderMan::trayTooltipStatusString(SyncResult::SyncPrepare, false, false, progress), u"Starting sync…"_s);
        progress->_status = ProgressInfo::Discovery;
        SyncFileItem item;
        item._type = ItemTypeDirectory;
        static_cast<void>(progress->recordDiscoveredItem(item));
        item._type = ItemTypeFile;
        for (auto i = 0; i < 1234; ++i) {
            static_cast<void>(progress->recordDiscoveredItem(item));
        }
        folder->progressInfo(*progress);
        const auto expected = u"Checking for changes — folders: 1, files: 1,234"_s;
        QCOMPARE(model.data(index, FolderStatusDelegate::SyncProgressOverallString).toString(), expected);
        QCOMPARE(model.data(index, FolderStatusDelegate::SyncProgressOverallPercent).toInt(), -1);
        QCOMPARE(model.data(index, Qt::ToolTipRole).toString(), expected);
        QCOMPARE(FolderMan::trayTooltipStatusString(SyncResult::SyncPrepare, false, false, progress), expected);
        QVERIFY(QMetaObject::invokeMethod(&model, "slotFolderSyncStateChange", Qt::DirectConnection, Q_ARG(OCC::Folder *, folder)));
        QCOMPARE(model.data(index, FolderStatusDelegate::SyncProgressOverallString).toString(), expected);

        const auto savePreview = [&](const QString &name) {
            const auto directory = qEnvironmentVariable("NEXTCLOUD_PROGRESS_PREVIEW_DIR");
            if (!directory.isEmpty()) {
                QCoreApplication::processEvents();
                QVERIFY(view.grab().save(QDir(directory).filePath(name + u".png"_s)));
            }
        };
        savePreview(u"discovery"_s);
        progress->_status = ProgressInfo::Reconcile;
        folder->progressInfo(*progress);
        QCOMPARE(model.data(index, FolderStatusDelegate::SyncProgressOverallString).toString(), progress->preparationStatusString());
        savePreview(u"reconcile"_s);
        accountState->setStateForTesting(AccountState::Disconnected);
        QVERIFY(QMetaObject::invokeMethod(&model, "slotFolderSyncStateChange", Qt::DirectConnection, Q_ARG(OCC::Folder *, folder)));
        QCOMPARE(model.data(index, FolderStatusDelegate::SyncProgressOverallString).toString(), u"Waiting for a connection…"_s);
        savePreview(u"waiting"_s);
        accountState->setStateForTesting(AccountState::NetworkError);
        QVERIFY(QMetaObject::invokeMethod(&model, "slotFolderSyncStateChange", Qt::DirectConnection, Q_ARG(OCC::Folder *, folder)));
        QCOMPARE(model.data(index, FolderStatusDelegate::SyncProgressOverallString).toString(), u"Connection interrupted — waiting to retry…"_s);
        savePreview(u"retry"_s);
        accountState->setStateForTesting(AccountState::SignedOut);
        QVERIFY(QMetaObject::invokeMethod(&model, "slotFolderSyncStateChange", Qt::DirectConnection, Q_ARG(OCC::Folder *, folder)));
        QVERIFY(model.data(index, FolderStatusDelegate::SyncProgressOverallString).toString().isEmpty());
    }
};

QTEST_MAIN(TestDiscoveryProgress)
#include "testdiscoveryprogress.moc"
