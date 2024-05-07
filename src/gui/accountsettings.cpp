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
#include "folderstatusmodel.h"
#include "folderwizard/folderwizard.h"
#include "gui/accountmodalwidget.h"
#include "gui/models/models.h"
#include "gui/qmlutils.h"
#include "gui/selectivesyncwidget.h"
#include "gui/spaces/spaceimageprovider.h"
#include "guiutility.h"
#include "oauthloginwidget.h"
#include "quotainfo.h"
#include "scheduling/syncscheduler.h"
#include "settingsdialog.h"
#include "theme.h"

#include <QAction>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QtQuickWidgets/QtQuickWidgets>

namespace {
constexpr auto modalWidgetStretchedMarginC = 50;
}

namespace OCC {

Q_LOGGING_CATEGORY(lcAccountSettings, "gui.account.settings", QtInfoMsg)

AccountSettings::AccountSettings(const AccountStatePtr &accountState, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AccountSettings)
    , _wasDisabledBefore(false)
    , _accountState(accountState)
{
    ui->setupUi(this);

    _model = new FolderStatusModel(this);
    _model->setAccountState(_accountState);

    auto weightedModel = new QSortFilterProxyModel(this);
    weightedModel->setSourceModel(_model);
    weightedModel->setSortRole(static_cast<int>(FolderStatusModel::Roles::Priority));
    weightedModel->sort(0, Qt::DescendingOrder);

    _sortModel = weightedModel;

    ui->quickWidget->rootContext()->setContextProperty(QStringLiteral("ctx"), this);
    ui->quickWidget->engine()->addImageProvider(QStringLiteral("space"), new Spaces::SpaceImageProvider(_accountState->account()));
    QmlUtils::initQuickWidget(ui->quickWidget, QUrl(QStringLiteral("qrc:/qt/qml/org/ownCloud/gui/qml/FolderDelegate.qml")));

    connect(FolderMan::instance(), &FolderMan::folderListChanged, _model, &FolderStatusModel::resetFolders);
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
    connect(ui->accountToolButton, &QToolButton::clicked, this, [this] {
        QMenu *menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->setAccessibleName(tr("Account options menu"));
        menu->addAction(_accountState->isSignedOut() ? tr("Log in") : tr("Log out"), this, &AccountSettings::slotToggleSignInState);
        auto *reconnectAction = menu->addAction(tr("Reconnect"), this, [this] { _accountState->checkConnectivity(true); });
        reconnectAction->setEnabled(!_accountState->isConnected() && !_accountState->isSignedOut());
        menu->addAction(tr("Remove"), this, &AccountSettings::slotDeleteAccount);
        menu->popup(mapToGlobal(ui->accountToolButton->pos()));

        // set the focus for accessability
        menu->setFocus();
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

void AccountSettings::slotToggleSignInState()
{
    if (_accountState->isSignedOut()) {
        _accountState->signIn();
    } else {
        _accountState->signOutByUi();
    }
}

void AccountSettings::slotCustomContextMenuRequested(Folder *folder)
{
    // qpointer for async calls
    const auto isDeployed = folder->isDeployed();
    const auto addRemoveFolderAction = [isDeployed, folder, this](QMenu *menu) {
        Q_ASSERT(!isDeployed);
        return menu->addAction(tr("Remove folder sync connection"), this, [folder, this] { slotRemoveCurrentFolder(folder); });
    };


    auto *menu = new QMenu(ui->quickWidget);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(folder, &OCC::Folder::destroyed, menu, &QMenu::close);
    // Only allow removal if the item isn't in "ready" state.
    if (!folder->isReady() && !isDeployed) {
        addRemoveFolderAction(menu);
        menu->popup(QCursor::pos());
        return;
    }
    // Add an action to open the folder in the system's file browser:
    const QUrl folderUrl = QUrl::fromLocalFile(folder->path());
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
    if (folder->accountState()->account()->capabilities().privateLinkPropertyAvailable()) {
        QString path = folder->remotePathTrailingSlash();
        menu->addAction(CommonStrings::showInWebBrowser(), [path, davUrl = folder->webDavUrl(), this] {
            fetchPrivateLinkUrl(_accountState->account(), davUrl, path, this, [](const QUrl &url) { Utility::openBrowser(url, nullptr); });
        });
    }


    // Root-folder specific actions:

    menu->addSeparator();

    bool folderPaused = folder->syncPaused();
    bool folderConnected = folder->accountState()->isConnected();

    // qpointer for the async context menu
    if (OC_ENSURE(folder->isReady())) {
        if (!folderPaused) {
            QAction *ac = menu->addAction(tr("Force sync now"));
            if (folder->isSyncRunning()) {
                ac->setText(tr("Restart sync"));
            }
            ac->setEnabled(folderConnected);
            connect(ac, &QAction::triggered, this, [folder, this] { slotForceSyncCurrentFolder(folder); });
        }

        QAction *ac = menu->addAction(folderPaused ? tr("Resume sync") : tr("Pause sync"));
        connect(ac, &QAction::triggered, this, [folder, this] { slotEnableCurrentFolder(folder, true); });

        if (!isDeployed) {
            addRemoveFolderAction(menu);

            if (Theme::instance()->showVirtualFilesOption()) {
                if (folder->virtualFilesEnabled()) {
                    if (!Theme::instance()->forceVirtualFilesOption()) {
                        menu->addAction(tr("Disable virtual file support"), this, [folder, this] { slotDisableVfsCurrentFolder(folder); });
                    } else {
                        const auto mode = VfsPluginManager::instance().bestAvailableVfsMode();
                        if (FolderMan::instance()->checkVfsAvailability(folder->path(), mode)) {
                            if (mode == Vfs::WindowsCfApi) {
                                ac = menu->addAction(tr("Enable virtual file support"));
                                connect(ac, &QAction::triggered, this, [folder, this] { slotEnableVfsCurrentFolder(folder); });
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
        doForceSyncCurrentFolder(folder);
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
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, {QLatin1String("/")});
    }
    FolderMan::instance()->setSyncEnabled(true);
    FolderMan::instance()->scheduleAllFolders();
}

void AccountSettings::slotRemoveCurrentFolder(Folder *folder)
{
    qCInfo(lcAccountSettings) << "Remove Folder " << folder->path();
    QString shortGuiLocalPath = folder->shortGuiLocalPath();

    auto messageBox = new QMessageBox(QMessageBox::Question, tr("Confirm Folder Sync Connection Removal"),
        tr("<p>Do you really want to stop syncing the folder <i>%1</i>?</p>"
           "<p><b>Note:</b> This will <b>not</b> delete any files.</p>")
            .arg(shortGuiLocalPath),
        QMessageBox::NoButton, ocApp()->gui()->settingsDialog());
    messageBox->setAttribute(Qt::WA_DeleteOnClose);
    QPushButton *yesButton = messageBox->addButton(tr("Remove Folder Sync Connection"), QMessageBox::YesRole);
    messageBox->addButton(tr("Cancel"), QMessageBox::NoRole);
    connect(messageBox, &QMessageBox::finished, this, [messageBox, yesButton, folder] {
        if (messageBox->clickedButton() == yesButton) {
            FolderMan::instance()->removeFolder(folder);
        }
    });
    messageBox->open();
}

void AccountSettings::slotEnableVfsCurrentFolder(Folder *folder)
{
    if (OC_ENSURE(VfsPluginManager::instance().bestAvailableVfsMode() == Vfs::WindowsCfApi)) {
        if (!folder) {
            return;
        }
        qCInfo(lcAccountSettings) << "Enabling vfs support for folder" << folder->path();

        // Change the folder vfs mode and load the plugin
        folder->setVirtualFilesEnabled(true);

        // don't schedule the folder, it might not be ready yet.
        // it will schedule its self once set up
    }
}

void AccountSettings::slotDisableVfsCurrentFolder(Folder *folder)
{
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
    connect(msgBox, &QMessageBox::finished, msgBox, [msgBox, folder, acceptButton] {
        msgBox->deleteLater();
        if (msgBox->clickedButton() != acceptButton || !folder) {
            return;
        }

        qCInfo(lcAccountSettings) << "Disabling vfs support for folder" << folder->path();

        // Also wipes virtual files, schedules remote discovery
        folder->setVirtualFilesEnabled(false);
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

void AccountSettings::slotEnableCurrentFolder(Folder *folder, bool terminate)
{
    Q_ASSERT(folder);
    qCInfo(lcAccountSettings) << "Application: enable folder with alias " << folder->path();
    bool currentlyPaused = false;

    // this sets the folder status to disabled but does not interrupt it.
    currentlyPaused = folder->syncPaused();
    if (!currentlyPaused && !terminate) {
        // check if a sync is still running and if so, ask if we should terminate.
        if (folder->isSyncRunning()) { // its still running
            auto msgbox = new QMessageBox(QMessageBox::Question, tr("Sync Running"), tr("The sync operation is running.<br/>Do you want to stop it?"),
                QMessageBox::Yes | QMessageBox::No, this);
            msgbox->setAttribute(Qt::WA_DeleteOnClose);
            msgbox->setDefaultButton(QMessageBox::Yes);
            connect(msgbox, &QMessageBox::accepted, this, [folder = QPointer<Folder>(folder), this] {
                if (folder) {
                    slotEnableCurrentFolder(folder, true);
                }
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

void AccountSettings::slotForceSyncCurrentFolder(Folder *folder)
{
    if (Utility::internetConnectionIsMetered() && ConfigFile().pauseSyncWhenMetered()) {
        auto messageBox = new QMessageBox(QMessageBox::Question, tr("Internet connection is metered"),
            tr("Synchronization is paused because the Internet connection is a metered connection"
               "<p>Do you really want to force a Synchronization now?"),
            QMessageBox::Yes | QMessageBox::No, ocApp()->gui()->settingsDialog());
        messageBox->setAttribute(Qt::WA_DeleteOnClose);
        connect(messageBox, &QMessageBox::accepted, this, [folder = QPointer<Folder>(folder), this] {
            if (folder) {
                doForceSyncCurrentFolder(folder);
            }
        });
        ownCloudGui::raise();
        messageBox->open();
    } else {
        doForceSyncCurrentFolder(folder);
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
    widget->setVisible(true);
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
