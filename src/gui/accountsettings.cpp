/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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


#include "accountsettings.h"
#include "ui_accountsettings.h"


#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "application.h"
#include "common/utility.h"
#include "commonstrings.h"
#include "configfile.h"
#include "creds/httpcredentialsgui.h"
#include "folderman.h"
#include "folderstatusdelegate.h"
#include "folderstatusmodel.h"
#include "folderwizard/folderwizard.h"
#include "gui/accountmodalwidget.h"
#include "gui/models/models.h"
#include "gui/selectivesyncwidget.h"
#include "guiutility.h"
#include "loginrequireddialog.h"
#include "oauthloginwidget.h"
#include "quotainfo.h"
#include "scheduling/syncscheduler.h"
#include "settingsdialog.h"
#include "theme.h"

#include <QAction>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QGroupBox>
#include <QIcon>
#include <QKeySequence>
#include <QMessageBox>
#include <QNetworkInformation>
#include <QPropertyAnimation>
#include <QSortFilterProxyModel>
#include <QToolTip>
#include <QTreeView>


namespace {
constexpr auto modalWidgetStretchedMarginC = 50;
}

namespace OCC {

Q_LOGGING_CATEGORY(lcAccountSettings, "gui.account.settings", QtInfoMsg)


AccountSettings::AccountSettings(const AccountStatePtr &accountState, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AccountSettings)
    , _delegate(new FolderStatusDelegate(this))
    , _wasDisabledBefore(false)
    , _accountState(accountState)
{
    ui->setupUi(this);

    _model = new FolderStatusModel(this);
    _model->setAccountState(_accountState);

    auto weightedModel = new Models::WeightedQSortFilterProxyModel(this);
    weightedModel->setSourceModel(_model);
    weightedModel->setWeightedColumn(static_cast<int>(FolderStatusModel::Columns::Priority), Qt::DescendingOrder);
    weightedModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    weightedModel->sort(static_cast<int>(FolderStatusModel::Columns::HeaderRole), Qt::DescendingOrder);

    _sortModel = weightedModel;

    ui->_folderList->setModel(_sortModel);

    ui->_folderList->setItemDelegate(_delegate);

    createAccountToolbox();
    connect(ui->_folderList, &QWidget::customContextMenuRequested,
        this, &AccountSettings::slotCustomContextMenuRequested);
    connect(ui->_folderList, &QAbstractItemView::clicked, this, &AccountSettings::slotFolderListClicked);
    QAction *syncNowAction = new QAction(this);
    connect(syncNowAction, &QAction::triggered, this, &AccountSettings::slotScheduleCurrentFolder);
    addAction(syncNowAction);

    QAction *syncNowWithRemoteDiscovery = new QAction(this);
    connect(syncNowWithRemoteDiscovery, &QAction::triggered, this, &AccountSettings::slotScheduleCurrentFolderForceFullDiscovery);
    addAction(syncNowWithRemoteDiscovery);

    connect(FolderMan::instance(), &FolderMan::folderListChanged, _model, &FolderStatusModel::resetFolders);
    connect(this, &AccountSettings::folderChanged, _model, &FolderStatusModel::resetFolders);

    ui->connectLabel->clear();

    connect(_accountState.data(), &AccountState::stateChanged, this, &AccountSettings::slotAccountStateChanged);
    slotAccountStateChanged();

    connect(ui->addButton, &QPushButton::clicked, this, &AccountSettings::slotAddFolder);

    if (_accountState->supportsSpaces()) {
        ui->addButton->setText(tr("Add Space"));
    } else {
        ui->addButton->setText(tr("Add Folder"));
    }

    connect(_model, &FolderStatusModel::dataChanged, [this]() {
        ui->addButton->setVisible(!Theme::instance()->singleSyncFolder() || _model->rowCount() == 0);
    });

    connect(_accountState.get(), &AccountState::isSettingUpChanged, this, [this] {
        if (_accountState->isSettingUp()) {
            ui->spinner->startAnimation();
            ui->stackedWidget->setCurrentWidget(ui->loadingPage);
        } else {
            ui->spinner->stopAnimation();
            ui->stackedWidget->setCurrentWidget(ui->folderListPage);
        }
    });
    ui->stackedWidget->setCurrentWidget(ui->folderListPage);
}


void AccountSettings::createAccountToolbox()
{
    QMenu *menu = new QMenu(ui->_accountToolbox);

    _toggleSignInOutAction = new QAction(tr("Log out"), this);
    connect(_toggleSignInOutAction, &QAction::triggered, this, &AccountSettings::slotToggleSignInState);
    menu->addAction(_toggleSignInOutAction);

    _toggleReconnect = menu->addAction(tr("Reconnect"));
    connect(_toggleReconnect, &QAction::triggered, this, [this] {
        _accountState->checkConnectivity(true);
    });

    QAction *action = new QAction(tr("Remove"), this);
    menu->addAction(action);
    connect(action, &QAction::triggered, this, &AccountSettings::slotDeleteAccount);

    ui->_accountToolbox->setText(tr("Account") + QLatin1Char(' '));
    ui->_accountToolbox->setMenu(menu);
    ui->_accountToolbox->setPopupMode(QToolButton::InstantPopup);
}

Folder *AccountSettings::selectedFolder() const
{
    const QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    return _model->folder(_sortModel->mapToSource(selected));
}

void AccountSettings::slotToggleSignInState()
{
    if (_accountState->isSignedOut()) {
        _accountState->signIn();
    } else {
        _accountState->signOutByUi();
    }
}

void AccountSettings::slotCustomContextMenuRequested(const QPoint &pos)
{
    auto *tv = ui->_folderList;
    QModelIndex index = tv->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    const auto isDeployed = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::IsDeployed)).data().toBool();
    const auto addRemoveFolderAction = [isDeployed, this](QMenu *menu) {
        Q_ASSERT(!isDeployed);
        return menu->addAction(tr("Remove folder sync connection"), this, &AccountSettings::slotRemoveCurrentFolder);
    };

    // Only allow removal if the item isn't in "ready" state.
    if (!index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::IsReady)).data().toBool() && !isDeployed) {
        QMenu *menu = new QMenu(tv);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        addRemoveFolderAction(menu);
        menu->popup(QCursor::pos());
        return;
    }

    QMenu *menu = new QMenu(tv);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // Add an action to open the folder in the system's file browser:

    QUrl folderUrl;
    if (auto *folder = selectedFolder()) {
        folderUrl = QUrl::fromLocalFile(folder->path());
    }

    if (!folderUrl.isEmpty()) {
        QAction *ac = menu->addAction(CommonStrings::showInFileBrowser(), [folderUrl]() {
            qCInfo(lcAccountSettings) << "Opening local folder" << folderUrl;
            if (!QDesktopServices::openUrl(folderUrl)) {
                qCWarning(lcAccountSettings) << "QDesktopServices::openUrl failed for" << folderUrl;
            }
        });

        if (!QFile::exists(folderUrl.toLocalFile())) {
            ac->setEnabled(false);
        }
    }

    // Add an action to open the folder on the server in a webbrowser:

    if (auto folder = _model->folder(_sortModel->mapToSource(index))) {
        if (folder->accountState()->account()->capabilities().privateLinkPropertyAvailable()) {
            QString path = folder->remotePathTrailingSlash();
            menu->addAction(CommonStrings::showInWebBrowser(), [path, davUrl = folder->webDavUrl(), this] {
                fetchPrivateLinkUrl(_accountState->account(), davUrl, path, this, [](const QUrl &url) {
                    Utility::openBrowser(url, nullptr);
                });
            });
        }
    }

    // Root-folder specific actions:

    menu->addSeparator();

    tv->setCurrentIndex(index);
    bool folderPaused = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderSyncPaused)).data().toBool();
    bool folderConnected = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderAccountConnected)).data().toBool();

    // qpointer for the async context menu
    QPointer<Folder> folder = selectedFolder();
    if (OC_ENSURE(folder && folder->isReady())) {
        if (!folderPaused) {
            QAction *ac = menu->addAction(tr("Force sync now"));
            if (folder->isSyncRunning()) {
                ac->setText(tr("Restart sync"));
            }
            ac->setEnabled(folderConnected);
            connect(ac, &QAction::triggered, this, &AccountSettings::slotForceSyncCurrentFolder);
        }

        QAction *ac = menu->addAction(folderPaused ? tr("Resume sync") : tr("Pause sync"));
        connect(ac, &QAction::triggered, this, &AccountSettings::slotEnableCurrentFolder);

        if (!isDeployed) {
            addRemoveFolderAction(menu);

            if (Theme::instance()->showVirtualFilesOption()) {
                if (folder->virtualFilesEnabled()) {
                    if (!Theme::instance()->forceVirtualFilesOption()) {
                        menu->addAction(tr("Disable virtual file support"), this, &AccountSettings::slotDisableVfsCurrentFolder);
                    }
                } else {
                    const auto mode = VfsPluginManager::instance().bestAvailableVfsMode();
                    if (FolderMan::instance()->checkVfsAvailability(folder->path(), mode)) {
                        if (mode == Vfs::WindowsCfApi) {
                            ac = menu->addAction(tr("Enable virtual file support"));
                            connect(ac, &QAction::triggered, this, &AccountSettings::slotEnableVfsCurrentFolder);
                        }
                    }
                }
            }
        }
        if (!folder->virtualFilesEnabled()) {
            menu->addAction(tr("Choose what to sync"), this, [folder, this] { showSelectiveSyncDialog(folder); });
        }
        menu->popup(QCursor::pos());
    } else {
        menu->deleteLater();
    }
}

void AccountSettings::slotFolderListClicked(const QModelIndex &indx)
{
    // tries to find if we clicked on the '...' button.
    auto *tv = ui->_folderList;
    const auto pos = tv->mapFromGlobal(QCursor::pos());
    const auto rect = tv->visualRect(indx);
    if (QStyle::visualRect(layoutDirection(), rect, _delegate->computeOptionsButtonRect(rect).toRect()).contains(pos)) {
        slotCustomContextMenuRequested(pos);
        return;
    }
    if (_delegate->errorsListRect(tv->visualRect(indx), indx).contains(pos)) {
        emit showIssuesList();
        return;
    }
}

void AccountSettings::showSelectiveSyncDialog(Folder *folder)
{
    auto *selectiveSync = new SelectiveSyncWidget(_accountState->account(), this);
    selectiveSync->setDavUrl(folder->webDavUrl());
    bool ok;
    selectiveSync->setFolderInfo(
        folder->remotePath(), folder->displayName(), folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok));
    Q_ASSERT(ok);

    auto *modalWidget = new AccountModalWidget(tr("Choose what to sync"), selectiveSync, this);
    connect(modalWidget, &AccountModalWidget::accepted, this, [selectiveSync, folder, this] {
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, selectiveSync->createBlackList());
        Q_EMIT folderChanged();
    });
    addModalWidget(modalWidget);
}

void AccountSettings::slotAddFolder()
{
    FolderMan::instance()->setSyncEnabled(false); // do not start more syncs.

    FolderWizard *folderWizard = new FolderWizard(_accountState, this);
    folderWizard->setAttribute(Qt::WA_DeleteOnClose);

    connect(folderWizard, &QDialog::accepted, this, &AccountSettings::slotFolderWizardAccepted);
    connect(folderWizard, &QDialog::rejected, this, [] {
        qCInfo(lcAccountSettings) << "Folder wizard cancelled";
        FolderMan::instance()->setSyncEnabled(true);
    });

    addModalLegacyDialog(folderWizard, AccountSettings::ModalWidgetSizePolicy::Expanding);
}


void AccountSettings::slotFolderWizardAccepted()
{
    FolderWizard *folderWizard = qobject_cast<FolderWizard *>(sender());
    qCInfo(lcAccountSettings) << "Folder wizard completed";

    const auto config = folderWizard->result();

    auto folder = FolderMan::instance()->addFolderFromFolderWizardResult(_accountState, config);

    if (!config.selectiveSyncBlackList.isEmpty() && OC_ENSURE(folder && !config.useVirtualFiles)) {
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, config.selectiveSyncBlackList);

        // The user already accepted the selective sync dialog. everything is in the white list
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, { QLatin1String("/") });
        emit folderChanged();
    }
    FolderMan::instance()->setSyncEnabled(true);
    FolderMan::instance()->scheduleAllFolders();
}

void AccountSettings::slotRemoveCurrentFolder()
{
    auto folder = selectedFolder();
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    if (selected.isValid() && folder) {
        int row = selected.row();

        qCInfo(lcAccountSettings) << "Remove Folder " << folder->path();
        QString shortGuiLocalPath = folder->shortGuiLocalPath();

        auto messageBox = new QMessageBox(QMessageBox::Question,
            tr("Confirm Folder Sync Connection Removal"),
            tr("<p>Do you really want to stop syncing the folder <i>%1</i>?</p>"
               "<p><b>Note:</b> This will <b>not</b> delete any files.</p>")
                .arg(shortGuiLocalPath),
            QMessageBox::NoButton,
            ocApp()->gui()->settingsDialog());
        messageBox->setAttribute(Qt::WA_DeleteOnClose);
        QPushButton *yesButton =
            messageBox->addButton(tr("Remove Folder Sync Connection"), QMessageBox::YesRole);
        messageBox->addButton(tr("Cancel"), QMessageBox::NoRole);
        connect(messageBox, &QMessageBox::finished, this, [messageBox, yesButton, folder, row, this]{
            if (messageBox->clickedButton() == yesButton) {
                FolderMan::instance()->removeFolder(folder);
                _sortModel->removeRow(row);

                // single folder fix to show add-button and hide remove-button
                emit folderChanged();
            }
        });
        messageBox->open();
    }
}

void AccountSettings::slotEnableVfsCurrentFolder()
{
    QPointer<Folder> folder = selectedFolder();
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    if (!selected.isValid() || !folder) {
        return;
    }
    if (OC_ENSURE(VfsPluginManager::instance().bestAvailableVfsMode() == Vfs::WindowsCfApi)) {
        if (!folder) {
            return;
        }
        qCInfo(lcAccountSettings) << "Enabling vfs support for folder" << folder->path();

        // Change the folder vfs mode and load the plugin
        folder->setVirtualFilesEnabled(true);

        // don't schedule the folder, it might not be ready yet.
        // it will schedule its self once set up

        ui->_folderList->doItemsLayout();
    }
}

void AccountSettings::slotDisableVfsCurrentFolder()
{
    QPointer<Folder> folder = selectedFolder();
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    if (!selected.isValid() || !folder)
        return;

    auto msgBox = new QMessageBox(
        QMessageBox::Question,
        tr("Disable virtual file support?"),
        tr("This action will disable virtual file support. As a consequence contents of folders that "
           "are currently marked as 'available online only' will be downloaded."
           "\n\n"
           "The only advantage of disabling virtual file support is that the selective sync feature "
           "will become available again."
           "\n\n"
           "This action will abort any currently running synchronization."));
    auto acceptButton = msgBox->addButton(tr("Disable support"), QMessageBox::AcceptRole);
    msgBox->addButton(tr("Cancel"), QMessageBox::RejectRole);
    connect(msgBox, &QMessageBox::finished, msgBox, [this, msgBox, folder, acceptButton] {
        msgBox->deleteLater();
        if (msgBox->clickedButton() != acceptButton || !folder) {
            return;
        }

        qCInfo(lcAccountSettings) << "Disabling vfs support for folder" << folder->path();

        // Also wipes virtual files, schedules remote discovery
        folder->setVirtualFilesEnabled(false);

        ui->_folderList->doItemsLayout();
    });
    msgBox->open();
}

void AccountSettings::showConnectionLabel(const QString &message, QStringList errors)
{
    if (errors.isEmpty()) {
        ui->connectLabel->setText(message);
        ui->connectLabel->setToolTip(QString());
    } else {
        errors.prepend(message);
        const QString msg = errors.join(QLatin1String("\n"));
        qCDebug(lcAccountSettings) << msg;
        ui->connectLabel->setText(msg);
        ui->connectLabel->setToolTip(QString());
    }
    ui->accountStatus->setVisible(!message.isEmpty());
    ui->warningLabel->setVisible(!errors.isEmpty());
}

void AccountSettings::slotEnableCurrentFolder(bool terminate)
{
    auto folder = selectedFolder();

    if (folder) {
        qCInfo(lcAccountSettings) << "Application: enable folder with alias " << folder->path();
        bool currentlyPaused = false;

        // this sets the folder status to disabled but does not interrupt it.
        currentlyPaused = folder->syncPaused();
        if (!currentlyPaused && !terminate) {
            // check if a sync is still running and if so, ask if we should terminate.
            if (folder->isSyncRunning()) { // its still running
                auto msgbox = new QMessageBox(QMessageBox::Question, tr("Sync Running"),
                    tr("The sync operation is running.<br/>Do you want to stop it?"),
                    QMessageBox::Yes | QMessageBox::No, this);
                msgbox->setAttribute(Qt::WA_DeleteOnClose);
                msgbox->setDefaultButton(QMessageBox::Yes);
                connect(msgbox, &QMessageBox::accepted, this, [this]{
                    slotEnableCurrentFolder(true);
                });
                msgbox->open();
                return;
            }
        }

        // message box can return at any time while the thread keeps running,
        // so better check again after the user has responded.
        if (folder->isSyncRunning() && terminate) {
            folder->slotTerminateSync(tr("Sync paused by user"));
        }
        folder->slotNextSyncFullLocalDiscovery(); // ensure we don't forget about local errors
        folder->setSyncPaused(!currentlyPaused);

        // keep state for the icon setting.
        if (currentlyPaused)
            _wasDisabledBefore = true;

        _model->slotUpdateFolderState(folder);
    }
}

void AccountSettings::slotScheduleCurrentFolder()
{
    if (auto folder = selectedFolder()) {
        FolderMan::instance()->scheduler()->enqueueFolder(folder);
    }
}

void AccountSettings::slotScheduleCurrentFolderForceFullDiscovery()
{
    if (auto folder = selectedFolder()) {
        folder->slotWipeErrorBlacklist();
        folder->slotNextSyncFullLocalDiscovery();
        folder->journalDb()->forceRemoteDiscoveryNextSync();
        FolderMan::instance()->scheduler()->enqueueFolder(folder);
    }
}

void AccountSettings::slotForceSyncCurrentFolder()
{
    if (auto selectedFolder = this->selectedFolder()) {
        if (Utility::internetConnectionIsMetered() && ConfigFile().pauseSyncWhenMetered()) {
            auto messageBox = new QMessageBox(QMessageBox::Question, tr("Internet connection is metered"),
                tr("Synchronization is paused because the Internet connection is a metered connection"
                   "<p>Do you really want to force a Synchronization now?"),
                QMessageBox::Yes | QMessageBox::No, ocApp()->gui()->settingsDialog());
            messageBox->setAttribute(Qt::WA_DeleteOnClose);
            connect(messageBox, &QMessageBox::accepted, this, [this, selectedFolder] { doForceSyncCurrentFolder(selectedFolder); });
            ownCloudGui::raise();
            messageBox->open();
        } else {
            doForceSyncCurrentFolder(selectedFolder);
        }
    }
}

void AccountSettings::doForceSyncCurrentFolder(Folder *selectedFolder)
{
    // Prevent new sync starts
    FolderMan::instance()->scheduler()->stop();

    // Terminate and reschedule any running sync
    for (auto *folder : FolderMan::instance()->folders()) {
        if (folder->isSyncRunning()) {
            folder->slotTerminateSync(tr("User triggered force sync"));
            FolderMan::instance()->scheduler()->enqueueFolder(folder);
        }
    }

    selectedFolder->slotWipeErrorBlacklist(); // issue #6757
    selectedFolder->slotNextSyncFullLocalDiscovery(); // ensure we don't forget about local errors

    // Insert the selected folder at the front of the queue
    FolderMan::instance()->scheduler()->enqueueFolder(selectedFolder, SyncScheduler::Priority::High);

    // Restart scheduler
    FolderMan::instance()->scheduler()->start();
}

void AccountSettings::slotAccountStateChanged()
{
    const AccountState::State state = _accountState->state();
    const AccountPtr account = _accountState->account();

    // in 2023 there should never be credentials encoded in the url, but we never know...
    const auto safeUrl = account->url().adjusted(QUrl::RemoveUserInfo);

    FolderMan *folderMan = FolderMan::instance();
    for (auto *folder : folderMan->folders()) {
        _model->slotUpdateFolderState(folder);
    }

    const QString server = QStringLiteral("<a href=\"%1\">%1</a>")
                               .arg(Utility::escape(safeUrl.toString()));

    switch (state) {
    case AccountState::PausedDueToMetered:
        showConnectionLabel(tr("Sync to %1 is paused due to metered internet connection.").arg(server));
        break;
    case AccountState::Connected: {
        QStringList errors;
        if (account->serverSupportLevel() != Account::ServerSupportLevel::Supported) {
            errors << tr("The server version %1 is unsupported! Proceed at your own risk.").arg(account->capabilities().status().versionString());
        }
        showConnectionLabel(tr("Connected to %1.").arg(server), errors);
        break;
    }
    case AccountState::ServiceUnavailable:
        showConnectionLabel(tr("Server %1 is temporarily unavailable.").arg(server));
        break;
    case AccountState::MaintenanceMode:
        showConnectionLabel(tr("Server %1 is currently in maintenance mode.").arg(server));
        break;
    case AccountState::SignedOut:
        showConnectionLabel(tr("Signed out from %1.").arg(server));
        break;
    case AccountState::AskingCredentials: {
        showConnectionLabel(tr("Updating credentials for %1...").arg(server));
        break;
    }
    case AccountState::Connecting:
        showConnectionLabel(tr("Connecting to: %1.").arg(server));
        break;
    case AccountState::ConfigurationError:
        showConnectionLabel(tr("Server configuration error: %1.")
                                .arg(server),
            _accountState->connectionErrors());
        break;
    case AccountState::NetworkError:
        // don't display the error to the user, https://github.com/owncloud/client/issues/9790
        [[fallthrough]];
    case AccountState::Disconnected:
        showConnectionLabel(tr("Disconnected from: %1.").arg(server));
        break;
    }
    _toggleReconnect->setEnabled(!_accountState->isConnected() && !_accountState->isSignedOut());
    // set the correct label for the Account toolbox button
    if (_accountState->isSignedOut()) {
        _toggleSignInOutAction->setText(tr("Log in"));
    } else {
        _toggleSignInOutAction->setText(tr("Log out"));
    }

    if (state == AccountState::Connected) {
        ui->addButton->setEnabled(true);

        if (_accountState->supportsSpaces()) {
            ui->addButton->setText(tr("Add Space"));
            ui->addButton->setToolTip(tr("Click this button to add a Space."));
        } else {
            ui->addButton->setText(tr("Add Folder"));
            ui->addButton->setToolTip(tr("Click this button to add a folder to synchronize."));
        }
    } else {
        ui->addButton->setEnabled(false);

        if (_accountState->supportsSpaces()) {
            ui->addButton->setText(tr("Add Space"));
            ui->addButton->setToolTip(tr("You need to be connected to add a Space."));
        } else {
            ui->addButton->setText(tr("Add Folder"));
            ui->addButton->setToolTip(tr("You need to be connected to add a folder."));
        }
    }
}

AccountSettings::~AccountSettings()
{
    _goingDown = true;
    delete ui;
}

void AccountSettings::addModalLegacyDialog(QWidget *widget, ModalWidgetSizePolicy sizePolicy)
{
    // create a widget filling the stacked widget
    // this widget contains a wrapping group box with widget as content
    auto *outerWidget = new QWidget;
    auto *groupBox = new QGroupBox;

    switch (sizePolicy) {
    case ModalWidgetSizePolicy::Expanding: {
        auto *outerLayout = new QHBoxLayout(outerWidget);
        outerLayout->setContentsMargins(modalWidgetStretchedMarginC, modalWidgetStretchedMarginC, modalWidgetStretchedMarginC, modalWidgetStretchedMarginC);
        outerLayout->addWidget(groupBox);
        auto *layout = new QHBoxLayout(groupBox);
        layout->addWidget(widget);
    } break;
    case ModalWidgetSizePolicy::Minimum: {
        auto *outerLayout = new QGridLayout(outerWidget);
        outerLayout->addWidget(groupBox, 0, 0, Qt::AlignCenter);
        auto *layout = new QHBoxLayout(groupBox);
        layout->addWidget(widget);
    } break;
    }
    groupBox->setTitle(widget->windowTitle());

    ui->stackedWidget->addWidget(outerWidget);
    ui->stackedWidget->setCurrentWidget(outerWidget);

    // the widget is supposed to behave like a dialog and we connect to its destuction
    Q_ASSERT(widget->testAttribute(Qt::WA_DeleteOnClose));
    connect(widget, &QWidget::destroyed, this, [this, outerWidget] {
        outerWidget->deleteLater();
        if (!_goingDown) {
            ocApp()->gui()->settingsDialog()->ceaseModality(_accountState->account().get());
        }
    });
    ocApp()->gui()->settingsDialog()->requestModality(_accountState->account().get());
}

void AccountSettings::addModalWidget(AccountModalWidget *widget)
{
    ui->stackedWidget->addWidget(widget);
    ui->stackedWidget->setCurrentWidget(widget);

    connect(widget, &AccountModalWidget::finished, this, [widget, this] {
        widget->deleteLater();
        ocApp()->gui()->settingsDialog()->ceaseModality(_accountState->account().get());
    });
    ocApp()->gui()->settingsDialog()->requestModality(_accountState->account().get());
}

void AccountSettings::slotDeleteAccount()
{
    // Deleting the account potentially deletes 'this', so
    // the QMessageBox should be destroyed before that happens.
    auto messageBox = new QMessageBox(QMessageBox::Question,
        tr("Confirm Account Removal"),
        tr("<p>Do you really want to remove the connection to the account <i>%1</i>?</p>"
           "<p><b>Note:</b> This will <b>not</b> delete any files.</p>")
            .arg(_accountState->account()->displayName()),
        QMessageBox::NoButton,
        this);
    auto yesButton = messageBox->addButton(tr("Remove connection"), QMessageBox::YesRole);
    messageBox->addButton(tr("Cancel"), QMessageBox::NoRole);
    messageBox->setAttribute(Qt::WA_DeleteOnClose);
    connect(messageBox, &QMessageBox::finished, this, [this, messageBox, yesButton]{
        if (messageBox->clickedButton() == yesButton) {
            auto manager = AccountManager::instance();
            manager->deleteAccount(_accountState);
            manager->save();
        }
    });
    messageBox->open();
}

bool AccountSettings::event(QEvent *e)
{
    if (e->type() == QEvent::Hide || e->type() == QEvent::Show) {
        if (!_accountState->supportsSpaces()) {
            _accountState->quotaInfo()->setActive(isVisible());
        }
    }
    return QWidget::event(e);
}

} // namespace OCC
