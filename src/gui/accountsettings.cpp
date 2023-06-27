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
#include "creds/abstractcredentials.h"
#include "creds/httpcredentialsgui.h"
#include "filesystem.h"
#include "folderman.h"
#include "folderstatusdelegate.h"
#include "folderstatusmodel.h"
#include "guiutility.h"
#include "quotainfo.h"
#include "settingsdialog.h"
#include "theme.h"
#include "tooltipupdater.h"

#include "folderwizard/folderwizard.h"

#include <math.h>

#include <QAction>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QIcon>
#include <QKeySequence>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QSortFilterProxyModel>
#include <QToolTip>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVariant>


#include "account.h"
#include "askexperimentalvirtualfilesfeaturemessagebox.h"
#include "gui/models/models.h"
#include "loginrequireddialog.h"
#include "oauthloginwidget.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcAccountSettings, "gui.account.settings", QtInfoMsg)

/**
 * Adjusts the mouse cursor based on the region it is on over the folder tree view.
 *
 * Used to show that one can click the red error list box by changing the cursor
 * to the pointing hand.
 */
class MouseCursorChanger : public QObject
{
    Q_OBJECT
public:
    MouseCursorChanger(QObject *parent)
        : QObject(parent)
    {
    }

    QTreeView *folderList;
    FolderStatusDelegate *delegate;
    QAbstractItemModel *model;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::HoverMove) {
            Qt::CursorShape shape = Qt::ArrowCursor;
            auto pos = folderList->mapFromGlobal(QCursor::pos());
            auto index = folderList->indexAt(pos);
            const auto rect = folderList->visualRect(index);
            if (index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::ItemType)).data().value<FolderStatusModel::ItemType>()
                    == FolderStatusModel::RootFolder
                && (QStyle::visualRect(folderList->layoutDirection(), rect, delegate->errorsListRect(rect, index).toRect()).contains(pos)
                    || QStyle::visualRect(folderList->layoutDirection(), rect, delegate->computeOptionsButtonRect(rect).toRect()).contains(pos))) {
                shape = Qt::PointingHandCursor;
            }
            folderList->setCursor(shape);
        }
        return QObject::eventFilter(watched, event);
    }
};

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
    weightedModel->setWeightedColumn(static_cast<int>(FolderStatusModel::Columns::Priority));
    weightedModel->setSortCaseSensitivity(Qt::CaseInsensitive);

    _sortModel = weightedModel;

    ui->_folderList->setModel(_sortModel);

    ui->_folderList->setItemDelegate(_delegate);

    for (int i = 1; i <= _sortModel->columnCount(); ++i) {
        ui->_folderList->header()->hideSection(i);
    }
    ui->_folderList->header()->setStretchLastSection(true);

    ui->_folderList->sortByColumn(static_cast<int>(FolderStatusModel::Columns::HeaderRole), Qt::AscendingOrder);

    ui->_folderList->header()->hide();
#if defined(Q_OS_MAC)
    ui->_folderList->setMinimumWidth(400);
#else
    ui->_folderList->setMinimumWidth(300);
#endif
    new ToolTipUpdater(ui->_folderList);

    auto mouseCursorChanger = new MouseCursorChanger(this);
    mouseCursorChanger->folderList = ui->_folderList;
    mouseCursorChanger->delegate = _delegate;
    mouseCursorChanger->model = _sortModel;
    ui->_folderList->setMouseTracking(true);
    ui->_folderList->setAttribute(Qt::WA_Hover, true);
    ui->_folderList->installEventFilter(mouseCursorChanger);

    ui->selectiveSyncStatus->hide();

    createAccountToolbox();
    connect(ui->_folderList, &QWidget::customContextMenuRequested,
        this, &AccountSettings::slotCustomContextMenuRequested);
    connect(ui->_folderList, &QAbstractItemView::clicked,
        this, &AccountSettings::slotFolderListClicked);
    connect(ui->_folderList, &QTreeView::expanded, this, &AccountSettings::refreshSelectiveSyncStatus);
    connect(ui->_folderList, &QTreeView::collapsed, this, &AccountSettings::refreshSelectiveSyncStatus);
    connect(ui->selectiveSyncNotification, &QLabel::linkActivated,
        this, &AccountSettings::slotLinkActivated);
    QAction *syncNowAction = new QAction(this);
    syncNowAction->setShortcut(QKeySequence(Qt::Key_F6));
    connect(syncNowAction, &QAction::triggered, this, &AccountSettings::slotScheduleCurrentFolder);
    addAction(syncNowAction);

    QAction *syncNowWithRemoteDiscovery = new QAction(this);
    syncNowWithRemoteDiscovery->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F6));
    connect(syncNowWithRemoteDiscovery, &QAction::triggered, this, &AccountSettings::slotScheduleCurrentFolderForceFullDiscovery);
    addAction(syncNowWithRemoteDiscovery);


    connect(_model, &FolderStatusModel::suggestExpand, this, [this](const QModelIndex &index) {
        ui->_folderList->expand(_sortModel->mapFromSource(index));
    });
    connect(_model, &FolderStatusModel::dirtyChanged, this, &AccountSettings::refreshSelectiveSyncStatus);
    refreshSelectiveSyncStatus();

    connect(ui->selectiveSyncApply, &QAbstractButton::clicked, _model, &FolderStatusModel::slotApplySelectiveSync);
    connect(ui->selectiveSyncCancel, &QAbstractButton::clicked, _model, &FolderStatusModel::resetFolders);
    connect(ui->bigFolderApply, &QAbstractButton::clicked, _model, &FolderStatusModel::slotApplySelectiveSync);
    connect(ui->bigFolderSyncAll, &QAbstractButton::clicked, _model, &FolderStatusModel::slotSyncAllPendingBigFolders);
    connect(ui->bigFolderSyncNone, &QAbstractButton::clicked, _model, &FolderStatusModel::slotSyncNoPendingBigFolders);

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

void AccountSettings::doExpand()
{
    // Make sure at least the root items are expanded
    for (int i = 0; i < _sortModel->rowCount(); ++i) {
        auto idx = _sortModel->index(i, 0);
        if (!ui->_folderList->isExpanded(idx))
            ui->_folderList->setExpanded(idx, true);
    }
}

void AccountSettings::slotCustomContextMenuRequested(const QPoint &pos)
{

    QTreeView *tv = ui->_folderList;
    QModelIndex index = tv->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    const auto isDeployed = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::IsDeployed)).data().toBool();
    const auto addRemoveFolderAction = [isDeployed, this](QMenu *menu) {
        Q_ASSERT(!isDeployed);
        return menu->addAction(tr("Remove folder sync connection"), this, &AccountSettings::slotRemoveCurrentFolder);
    };

    auto classification = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::ItemType)).data().value<FolderStatusModel::ItemType>();
    if (classification != FolderStatusModel::RootFolder && classification != FolderStatusModel::SubFolder) {
        return;
    }

    // Only allow removal if the item isn't in "ready" state.
    if (classification == FolderStatusModel::RootFolder && !index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::IsReady)).data().toBool() && !isDeployed) {
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
    if (classification == FolderStatusModel::SubFolder) {
        const QString fileName = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderPathRole)).data().toString();
        folderUrl = QUrl::fromLocalFile(fileName);
    } else {
        // the root folder
        if (auto *folder = selectedFolder()) {
            folderUrl = QUrl::fromLocalFile(folder->path());
        }
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

    if (auto info = _model->infoForIndex(_sortModel->mapToSource(index))) {
        if (info->_folder->accountState()->account()->capabilities().privateLinkPropertyAvailable()) {
            QString path = info->_folder->remotePathTrailingSlash();
            if (classification == FolderStatusModel::SubFolder) {
                // Only add the path of subfolders, because the remote path is the path of the root folder.
                path += info->_path;
            }
            menu->addAction(CommonStrings::showInWebBrowser(), [path, davUrl = info->_folder->webDavUrl(), this] {
                fetchPrivateLinkUrl(_accountState->account(), davUrl, path, this, [](const QUrl &url) {
                    Utility::openBrowser(url, nullptr);
                });
            });
        }
    }

    // For sub-folders we're now done.

    if (index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::ItemType)).data().value<FolderStatusModel::ItemType>() == FolderStatusModel::SubFolder) {
        menu->popup(QCursor::pos());
        return;
    }

    // Root-folder specific actions:

    menu->addSeparator();

    tv->setCurrentIndex(index);
    bool folderPaused = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderSyncPaused)).data().toBool();
    bool folderConnected = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderAccountConnected)).data().toBool();

    // qpointer for the async context menu
    QPointer<Folder> folder = selectedFolder();
    if (OC_ENSURE(folder && folder->isReady())) {
        if (!ui->_folderList->isExpanded(index) && folder->supportsSelectiveSync()) {
            QAction *ac = menu->addAction(tr("Choose what to sync"));
            ac->setEnabled(folderConnected);
            connect(ac, &QAction::triggered, this, &AccountSettings::doExpand);
        }

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
                        menu->addAction(tr("Disable virtual file support..."), this, &AccountSettings::slotDisableVfsCurrentFolder);
                    }
                } else {
                    const auto mode = VfsPluginManager::instance().bestAvailableVfsMode();
                    if (FolderMan::instance()->checkVfsAvailability(folder->path(), mode)) {
                        if (mode == Vfs::WindowsCfApi || (Theme::instance()->enableExperimentalFeatures() && mode != Vfs::Off)) {
                            ac = menu->addAction(tr("Enable virtual file support%1...").arg(mode == Vfs::WindowsCfApi ? QString() : tr(" (experimental)")));
                            connect(ac, &QAction::triggered, this, &AccountSettings::slotEnableVfsCurrentFolder);
                        }
                    }
                }
            }
        }
        menu->popup(QCursor::pos());
    } else {
        menu->deleteLater();
    }
}

void AccountSettings::slotFolderListClicked(const QModelIndex &indx)
{
    const auto itemType = indx.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::ItemType)).data().value<FolderStatusModel::ItemType>();
    if (itemType == FolderStatusModel::RootFolder) {
        // tries to find if we clicked on the '...' button.
        QTreeView *tv = ui->_folderList;
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

        // Expand root items on single click
        if (_accountState && _accountState->state() == AccountState::Connected) {
            bool expanded = !(ui->_folderList->isExpanded(indx));
            ui->_folderList->setExpanded(indx, expanded);
        }
    }
}

void AccountSettings::slotAddFolder()
{
    FolderMan::instance()->setSyncEnabled(false); // do not start more syncs.

    FolderWizard *folderWizard = new FolderWizard(_accountState, ocApp()->gui()->settingsDialog());
    folderWizard->setAttribute(Qt::WA_DeleteOnClose);
    folderWizard->resize(ocApp()->gui()->settingsDialog()->sizeHintForChild());

    connect(folderWizard, &QDialog::accepted, this, &AccountSettings::slotFolderWizardAccepted);
    connect(folderWizard, &QDialog::rejected, this, [] {
        qCInfo(lcAccountSettings) << "Folder wizard cancelled";
        FolderMan::instance()->setSyncEnabled(true);
    });
    folderWizard->open();
    ocApp()->gui()->raiseDialog(folderWizard);
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
        ownCloudGui::raiseDialog(messageBox);
    }
}

void AccountSettings::slotEnableVfsCurrentFolder()
{
    FolderMan *folderMan = FolderMan::instance();
    QPointer<Folder> folder = selectedFolder();
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    if (!selected.isValid() || !folder)
        return;

    auto messageBox = new AskExperimentalVirtualFilesFeatureMessageBox(ocApp()->gui()->settingsDialog());

    connect(messageBox, &AskExperimentalVirtualFilesFeatureMessageBox::accepted, this, [this, folder]() {
        if (!folder) {
            return;
        }

#ifdef Q_OS_WIN
        // we might need to add or remove the panel entry as cfapi brings this feature out of the box
        FolderMan::instance()->navigationPaneHelper().scheduleUpdateCloudStorageRegistry();
#endif

        // It is unsafe to switch on vfs while a sync is running - wait if necessary.
        auto connection = std::make_shared<QMetaObject::Connection>();
        auto switchVfsOn = [folder, connection, this]() {
            if (*connection)
                QObject::disconnect(*connection);

            qCInfo(lcAccountSettings) << "Enabling vfs support for folder" << folder->path();

            // Wipe selective sync blacklist
            bool ok = false;
            const auto oldBlacklist = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok);
            folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, {});

            // Change the folder vfs mode and load the plugin
            folder->setVirtualFilesEnabled(true);
            folder->setVfsOnOffSwitchPending(false);

            for (const auto &entry : oldBlacklist) {
                folder->journalDb()->schedulePathForRemoteDiscovery(entry);
                folder->vfs().setPinState(entry, PinState::OnlineOnly);
            }
            folder->slotNextSyncFullLocalDiscovery();

            FolderMan::instance()->scheduleFolder(folder);

            ui->_folderList->doItemsLayout();
            ui->selectiveSyncStatus->setVisible(false);
        };

        if (folder->isSyncRunning()) {
            *connection = connect(folder, &Folder::syncFinished, this, switchVfsOn);
            folder->setVfsOnOffSwitchPending(true);
            folder->slotTerminateSync();
            ui->_folderList->doItemsLayout();
        } else {
            switchVfsOn();
        }
    });

    // no need to show the message box on Windows
    // as a little shortcut, we just re-use the message box's accept handler
    if (VfsPluginManager::instance().bestAvailableVfsMode() == Vfs::WindowsCfApi) {
        Q_EMIT messageBox->accepted();
    } else {
        messageBox->show();
        ocApp()->gui()->raiseDialog(messageBox);
    }
}

void AccountSettings::slotDisableVfsCurrentFolder()
{
    FolderMan *folderMan = FolderMan::instance();
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
        if (msgBox->clickedButton() != acceptButton|| !folder)
            return;

#ifdef Q_OS_WIN
         // we might need to add or remove the panel entry as cfapi brings this feature out of the box
        FolderMan::instance()->navigationPaneHelper().scheduleUpdateCloudStorageRegistry();
#endif

        // It is unsafe to switch off vfs while a sync is running - wait if necessary.
        auto connection = std::make_shared<QMetaObject::Connection>();
        auto switchVfsOff = [folder, connection, this]() {
            if (*connection)
                QObject::disconnect(*connection);

            qCInfo(lcAccountSettings) << "Disabling vfs support for folder" << folder->path();

            // Also wipes virtual files, schedules remote discovery
            folder->setVirtualFilesEnabled(false);
            folder->setVfsOnOffSwitchPending(false);
            folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, {});

            ui->_folderList->doItemsLayout();
        };

        if (folder->isSyncRunning()) {
            *connection = connect(folder, &Folder::syncFinished, this, switchVfsOff);
            folder->setVfsOnOffSwitchPending(true);
            folder->slotTerminateSync();
            ui->_folderList->doItemsLayout();
        } else {
            switchVfsOff();
        }
    });
    msgBox->open();
}

void AccountSettings::showConnectionLabel(const QString &message, QStringList errors)
{
    const QString errStyle = QStringLiteral("color:#ffffff; background-color:#bb4d4d;padding:5px;"
                                            "border-width: 1px; border-style: solid; border-color: #aaaaaa;"
                                            "border-radius:5px;");
    if (errors.isEmpty()) {
        ui->connectLabel->setText(message);
        ui->connectLabel->setToolTip(QString());
        ui->connectLabel->setStyleSheet(QString());
    } else {
        errors.prepend(message);
        const QString msg = errors.join(QLatin1String("\n"));
        qCDebug(lcAccountSettings) << msg;
        ui->connectLabel->setText(msg);
        ui->connectLabel->setToolTip(QString());
        ui->connectLabel->setStyleSheet(errStyle);
    }
    ui->accountStatus->setVisible(!message.isEmpty());
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
            folder->slotTerminateSync();
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
        FolderMan::instance()->scheduleFolder(folder);
    }
}

void AccountSettings::slotScheduleCurrentFolderForceFullDiscovery()
{
    if (auto folder = selectedFolder()) {
        folder->slotWipeErrorBlacklist();
        folder->slotNextSyncFullLocalDiscovery();
        folder->journalDb()->forceRemoteDiscoveryNextSync();
        FolderMan::instance()->scheduleFolder(folder);
    }
}

void AccountSettings::slotForceSyncCurrentFolder()
{
    if (auto selectedFolder = this->selectedFolder()) {
        // Terminate and reschedule any running sync
        for (auto *folder : FolderMan::instance()->folders()) {
            if (folder->isSyncRunning()) {
                folder->slotTerminateSync();
                FolderMan::instance()->scheduleFolder(folder);
            }
        }

        selectedFolder->slotWipeErrorBlacklist(); // issue #6757
        selectedFolder->slotNextSyncFullLocalDiscovery(); // ensure we don't forget about local errors
        // Insert the selected folder at the front of the queue
        FolderMan::instance()->scheduleFolder(selectedFolder, true);
    }
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
    case AccountState::Connected: {
        QStringList errors;
        if (account->serverSupportLevel() != Account::ServerSupportLevel::Supported) {
            errors << tr("The server version %1 is unsupported! Proceed at your own risk.").arg(account->capabilities().status().versionString());
        }
        showConnectionLabel(tr("Connected to %1.").arg(server), errors);
        if (_askForOAuthLoginDialog != nullptr) {
            _askForOAuthLoginDialog->accept();
        }
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
        auto cred = qobject_cast<HttpCredentialsGui *>(account->credentials());
        if (cred && cred->isUsingOAuth()) {
            if (_askForOAuthLoginDialog != nullptr) {
                qCDebug(lcAccountSettings) << "ask for OAuth login dialog is shown already";
                return;
            }

            qCDebug(lcAccountSettings) << "showing modal dialog asking user to log in again via OAuth2";

            _askForOAuthLoginDialog = new LoginRequiredDialog(LoginRequiredDialog::Mode::OAuth, ocApp()->gui()->settingsDialog());

            // make sure it's cleaned up since it's not owned by the account settings (also prevents memory leaks)
            _askForOAuthLoginDialog->setAttribute(Qt::WA_DeleteOnClose);

            _askForOAuthLoginDialog->setTopLabelText(tr("The account %1 is currently logged out.\n\nPlease authenticate using your browser.").arg(account->displayName()));

            auto *contentWidget = qobject_cast<OAuthLoginWidget *>(_askForOAuthLoginDialog->contentWidget());

            connect(contentWidget, &OAuthLoginWidget::copyUrlToClipboardButtonClicked, _askForOAuthLoginDialog, [account]() {
                // TODO: use authorisationLinkAsync
                auto link = qobject_cast<HttpCredentialsGui *>(account->credentials())->authorisationLink().toString();
                qApp->clipboard()->setText(link);
            });

            connect(contentWidget, &OAuthLoginWidget::openBrowserButtonClicked, _askForOAuthLoginDialog, [cred]() {
                cred->openBrowser();
            });

            contentWidget->setEnabled(false);
            connect(cred, &HttpCredentialsGui::authorisationLinkChanged, contentWidget, [contentWidget]() {
                contentWidget->setEnabled(true);
            });

            connect(
                cred, &HttpCredentialsGui::authorisationLinkChanged,
                this, &AccountSettings::slotAccountStateChanged,
                Qt::UniqueConnection);

            connect(_askForOAuthLoginDialog, &LoginRequiredDialog::rejected, this, [this]() {
                // if a user dismisses the dialog, we have no choice but signing them out
                _accountState->signOutByUi();
            });

            connect(contentWidget, &OAuthLoginWidget::retryButtonClicked, _askForOAuthLoginDialog, [contentWidget, accountPtr = account]() {
                auto creds = qobject_cast<HttpCredentialsGui *>(accountPtr->credentials());
                creds->restartOAuth();
                contentWidget->hideRetryFrame();
            });

            connect(cred, &HttpCredentialsGui::oAuthErrorOccurred, _askForOAuthLoginDialog, [loginDialog = _askForOAuthLoginDialog, contentWidget, cred]() {
                Q_ASSERT(!cred->ready());

                ocApp()->gui()->raiseDialog(loginDialog);
                contentWidget->showRetryFrame();
            });

            showConnectionLabel(tr("Reauthorization required."));

            _askForOAuthLoginDialog->open();
            ocApp()->gui()->raiseDialog(_askForOAuthLoginDialog);

            QTimer::singleShot(0, [contentWidget]() {
                contentWidget->setFocus(Qt::OtherFocusReason);
            });
        } else {
            showConnectionLabel(tr("Connecting to %1...").arg(server));
        }
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

    // Disabling expansion of folders might require hiding the selective
    // sync user interface buttons.
    refreshSelectiveSyncStatus();

    _toggleReconnect->setEnabled(!_accountState->isConnected() && !_accountState->isSignedOut());
    // set the correct label for the Account toolbox button
    if (_accountState->isSignedOut()) {
        _toggleSignInOutAction->setText(tr("Log in"));
    } else {
        _toggleSignInOutAction->setText(tr("Log out"));
    }

    ui->addButton->setEnabled(state == AccountState::Connected);
    if (state == AccountState::Connected) {
        ui->_folderList->setItemsExpandable(true);
        if (_accountState->supportsSpaces()) {
            ui->addButton->setText(tr("Add Space"));
            ui->addButton->setToolTip(tr("Click this button to add a Space."));
        } else {
            ui->addButton->setText(tr("Add Folder"));
            ui->addButton->setToolTip(tr("Click this button to add a folder to synchronize."));
        }
    } else {
        ui->_folderList->setItemsExpandable(false);
        ui->addButton->setText(tr("Add Folder"));
        ui->addButton->setToolTip(tr("You need to be connected to add a folder."));

        /* check if there are expanded root items, if so, close them */
        ui->_folderList->collapseAll();
    }
}

void AccountSettings::slotLinkActivated(const QString &link)
{
    // Parse folder alias and filename from the link, calculate the index
    // and select it if it exists.
    const QStringList li = link.split(QStringLiteral("?folder="));
    if (li.count() > 1) {
        QString myFolder = li[0];
        const QByteArray id = QUrl::fromPercentEncoding(li[1].toUtf8()).toUtf8();
        if (myFolder.endsWith(QLatin1Char('/')))
            myFolder.chop(1);

        // Make sure the folder itself is expanded
        Folder *folder = FolderMan::instance()->folder(id);
        if (folder) {
            QModelIndex folderIndx = _sortModel->mapFromSource(_model->indexForPath(folder, QString()));
            if (!ui->_folderList->isExpanded(folderIndx)) {
                ui->_folderList->setExpanded(folderIndx, true);
            }

            QModelIndex indx = _sortModel->mapFromSource(_model->indexForPath(folder, myFolder));
            if (indx.isValid()) {
                // make sure all the parents are expanded
                for (auto i = indx.parent(); i.isValid(); i = i.parent()) {
                    if (!ui->_folderList->isExpanded(i)) {
                        ui->_folderList->setExpanded(i, true);
                    }
                }
                ui->_folderList->setSelectionMode(QAbstractItemView::SingleSelection);
                ui->_folderList->setCurrentIndex(indx);
                ui->_folderList->scrollTo(indx);
            } else {
                qCWarning(lcAccountSettings) << "Unable to find a valid index for " << myFolder;
            }
        }
    }
}

AccountSettings::~AccountSettings()
{
    delete ui;
}

void AccountSettings::refreshSelectiveSyncStatus()
{
    QString msg;
    int cnt = 0;
    for (Folder *folder : FolderMan::instance()->folders()) {
        if (folder->accountState() != _accountState || !folder->isReady()) {
            continue;
        }

        bool ok;
        const auto &undecidedList = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, &ok);
        for (const auto &it : undecidedList) {
            // FIXME: add the folder alias in a hoover hint.
            // folder->alias() + QLatin1String("/")
            if (cnt++) {
                msg += QLatin1String(", ");
            }
            QString myFolder = (it);
            if (myFolder.endsWith(QLatin1Char('/'))) {
                myFolder.chop(1);
            }
            QModelIndex theIndx = _sortModel->mapFromSource(_model->indexForPath(folder, myFolder));
            if (theIndx.isValid()) {
                msg += QStringLiteral("<a href=\"%1?folder=%2\">%1</a>")
                           .arg(Utility::escape(myFolder), QString::fromUtf8(QUrl::toPercentEncoding(QString::fromUtf8(folder->id()))));
            } else {
                msg += myFolder; // no link because we do not know the index yet.
            }
        }
    }

    // Some selective sync ui (either normal editing or big folder) will show
    // if this variable ends up true.
    bool shouldBeVisible = false;

    if (msg.isEmpty()) {
        // Show the ui if the model is dirty only
        shouldBeVisible = _model->isDirty() && _accountState->isConnected();

        ui->selectiveSyncButtons->setVisible(true);
        ui->bigFolderUi->setVisible(false);
        ui->selectiveSyncApply->setEnabled(_model->isDirty());
    } else {
        // There's a reason the big folder ui should be shown
        shouldBeVisible = _accountState->isConnected();

        ConfigFile cfg;
        QString info = !cfg.confirmExternalStorage()
            ? tr("There are folders that were not synchronized because they are too big: ")
            : !cfg.newBigFolderSizeLimit().first
                ? tr("There are folders that were not synchronized because they are external storages: ")
                : tr("There are folders that were not synchronized because they are too big or external storages: ");

        ui->selectiveSyncNotification->setText(info + msg);
        ui->selectiveSyncButtons->setVisible(false);
        ui->bigFolderUi->setVisible(true);
        ui->bigFolderApply->setEnabled(_model->isDirty());
    }

    bool wasVisible = !ui->selectiveSyncStatus->isHidden();
    if (wasVisible != shouldBeVisible) {
        QSize hint = ui->selectiveSyncStatus->sizeHint();
        if (shouldBeVisible) {
            ui->selectiveSyncStatus->setMaximumHeight(0);
            ui->selectiveSyncStatus->setVisible(true);
            doExpand();
        }
        auto anim = new QPropertyAnimation(ui->selectiveSyncStatus, "maximumHeight", ui->selectiveSyncStatus);
        anim->setEndValue(shouldBeVisible ? hint.height() : 0);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        connect(anim, &QPropertyAnimation::finished, this, [this, shouldBeVisible]() {
            ui->selectiveSyncStatus->setMaximumHeight(QWIDGETSIZE_MAX);
            if (!shouldBeVisible) {
                ui->selectiveSyncStatus->hide();
            }
        });
    }
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
    if (e->type() == QEvent::Show) {
        // Expand the folder automatically only if there's only one, see #4283
        // The 2 is 1 folder + 1 'add folder' button
        if (_sortModel->rowCount() <= 2) {
            ui->_folderList->setExpanded(_sortModel->index(0, 0), true);
        }
    }
    return QWidget::event(e);
}

} // namespace OCC

#include "accountsettings.moc"
