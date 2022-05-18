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

#include <QDesktopServices>
#include <QDir>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QAction>
#include <QVBoxLayout>
#include <QTreeView>
#include <QKeySequence>
#include <QIcon>
#include <QVariant>
#include <QToolTip>
#include <qstringlistmodel.h>
#include <qpropertyanimation.h>

#include "account.h"
#include "askexperimentalvirtualfilesfeaturemessagebox.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcAccountSettings, "gui.account.settings", QtInfoMsg)

static const char progressBarStyleC[] =
    "QProgressBar {"
    "border: 1px solid grey;"
    "border-radius: 5px;"
    "text-align: center;"
    "}"
    "QProgressBar::chunk {"
    "background-color: %1; width: 1px;"
    "}";

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
    FolderStatusModel *model;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::HoverMove) {
            Qt::CursorShape shape = Qt::ArrowCursor;
            auto pos = folderList->mapFromGlobal(QCursor::pos());
            auto index = folderList->indexAt(pos);
            if (model->classify(index) == FolderStatusModel::RootFolder
                && (FolderStatusDelegate::errorsListRect(folderList->visualRect(index), index).contains(pos)
                    || FolderStatusDelegate::optionsButtonRect(folderList->visualRect(index),folderList->layoutDirection()).contains(pos))) {
                shape = Qt::PointingHandCursor;
            }
            folderList->setCursor(shape);
        }
        return QObject::eventFilter(watched, event);
    }
};

AccountSettings::AccountSettings(AccountStatePtr accountState, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AccountSettings)
    , _wasDisabledBefore(false)
    , _accountState(accountState)
    , _quotaInfo(accountState)
{
    ui->setupUi(this);

    _model = new FolderStatusModel;
    _model->setAccountState(_accountState);
    _model->setParent(this);
    FolderStatusDelegate *delegate = new FolderStatusDelegate;
    delegate->setParent(this);

    ui->_folderList->header()->hide();
    ui->_folderList->setItemDelegate(delegate);
    ui->_folderList->setModel(_model);
#if defined(Q_OS_MAC)
    ui->_folderList->setMinimumWidth(400);
#else
    ui->_folderList->setMinimumWidth(300);
#endif
    new ToolTipUpdater(ui->_folderList);

    auto mouseCursorChanger = new MouseCursorChanger(this);
    mouseCursorChanger->folderList = ui->_folderList;
    mouseCursorChanger->model = _model;
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
    connect(_model, &FolderStatusModel::suggestExpand, ui->_folderList, &QTreeView::expand);
    connect(_model, &FolderStatusModel::dirtyChanged, this, &AccountSettings::refreshSelectiveSyncStatus);
    refreshSelectiveSyncStatus();
    connect(_model, &QAbstractItemModel::rowsInserted,
        this, &AccountSettings::refreshSelectiveSyncStatus);

    QAction *syncNowAction = new QAction(this);
    syncNowAction->setShortcut(QKeySequence(Qt::Key_F6));
    connect(syncNowAction, &QAction::triggered, this, &AccountSettings::slotScheduleCurrentFolder);
    addAction(syncNowAction);

    QAction *syncNowWithRemoteDiscovery = new QAction(this);
    syncNowWithRemoteDiscovery->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_F6));
    connect(syncNowWithRemoteDiscovery, &QAction::triggered, this, &AccountSettings::slotScheduleCurrentFolderForceFullDiscovery);
    addAction(syncNowWithRemoteDiscovery);


    connect(ui->selectiveSyncApply, &QAbstractButton::clicked, _model, &FolderStatusModel::slotApplySelectiveSync);
    connect(ui->selectiveSyncCancel, &QAbstractButton::clicked, _model, &FolderStatusModel::resetFolders);
    connect(ui->bigFolderApply, &QAbstractButton::clicked, _model, &FolderStatusModel::slotApplySelectiveSync);
    connect(ui->bigFolderSyncAll, &QAbstractButton::clicked, _model, &FolderStatusModel::slotSyncAllPendingBigFolders);
    connect(ui->bigFolderSyncNone, &QAbstractButton::clicked, _model, &FolderStatusModel::slotSyncNoPendingBigFolders);

    connect(FolderMan::instance(), &FolderMan::folderListChanged, _model, &FolderStatusModel::resetFolders);
    connect(this, &AccountSettings::folderChanged, _model, &FolderStatusModel::resetFolders);


    QColor color = palette().highlight().color();
    ui->quotaProgressBar->setStyleSheet(QString::fromLatin1(progressBarStyleC).arg(color.name()));

    ui->connectLabel->setText(tr("No account configured."));

    connect(_accountState.data(), &AccountState::stateChanged, this, &AccountSettings::slotAccountStateChanged);
    slotAccountStateChanged();

    connect(&_quotaInfo, &QuotaInfo::quotaUpdated,
        this, &AccountSettings::slotUpdateQuota);

    ui->openBrowserButton->setVisible(false);
    connect(ui->openBrowserButton, &QToolButton::clicked, this, [this]{
        qobject_cast<HttpCredentialsGui *>(_accountState->account()->credentials())->openBrowser();
    });
}


void AccountSettings::createAccountToolbox()
{
    QMenu *menu = new QMenu(ui->_accountToolbox);

    _toggleSignInOutAction = new QAction(tr("Log out"), this);
    connect(_toggleSignInOutAction, &QAction::triggered, this, &AccountSettings::slotToggleSignInState);
    menu->addAction(_toggleSignInOutAction);

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
    return _model->folder(selected);
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
    for (int i = 0; i < _model->rowCount(); ++i) {
        auto idx = _model->index(i);
        if (!ui->_folderList->isExpanded(idx))
            ui->_folderList->setExpanded(idx, true);
    }
}

void AccountSettings::slotCustomContextMenuRequested(const QPoint &pos)
{
    const auto removeFolderAction = [this](QMenu *menu) {
        return menu->addAction(tr("Remove folder sync connection"), this, &AccountSettings::slotRemoveCurrentFolder);
    };

    QTreeView *tv = ui->_folderList;
    QModelIndex index = tv->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    auto classification = _model->classify(index);
    if (classification != FolderStatusModel::RootFolder && classification != FolderStatusModel::SubFolder) {
        return;
    }


    // Only allow removal if the item isn't in "ready" state.
    if (classification == FolderStatusModel::RootFolder && !_model->data(index, FolderStatusDelegate::IsReady).toBool()) {
        QMenu *menu = new QMenu(tv);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        removeFolderAction(menu);
        menu->popup(QCursor::pos());
        return;
    }

    QMenu *menu = new QMenu(tv);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // Add an action to open the folder in the system's file browser:

    QUrl folderUrl;
    if (classification == FolderStatusModel::SubFolder) {
        QString fileName = _model->data(index, FolderStatusDelegate::FolderPathRole).toString();
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

    if (auto info = _model->infoForIndex(index)) {
        QString path = info->_folder->remotePathTrailingSlash();
        if (classification == FolderStatusModel::SubFolder) {
            // Only add the path of subfolders, because the remote path is the path of the root folder.
            path += info->_path;
        }
        menu->addAction(CommonStrings::showInWebBrowser(), [path, davUrl = info->_folder->webDavUrl(), this] {
            fetchPrivateLinkUrl(_accountState->account(), davUrl, path, this, [](const QString &url) {
                Utility::openBrowser(url, nullptr);
            });
        });
    }

    // For sub-folders we're now done.

    if (_model->classify(index) == FolderStatusModel::SubFolder) {
        menu->popup(QCursor::pos());
        return;
    }

    // Root-folder specific actions:

    menu->addSeparator();

    tv->setCurrentIndex(index);
    bool folderPaused = _model->data(index, FolderStatusDelegate::FolderSyncPaused).toBool();
    bool folderConnected = _model->data(index, FolderStatusDelegate::FolderAccountConnected).toBool();

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

        removeFolderAction(menu);

        if (folder->virtualFilesEnabled() && !Theme::instance()->forceVirtualFilesOption()) {
            menu->addAction(tr("Disable virtual file support..."), this, &AccountSettings::slotDisableVfsCurrentFolder);
        }

        if (Theme::instance()->showVirtualFilesOption()
            && !folder->virtualFilesEnabled() && FolderMan::instance()->checkVfsAvailability(folder->path())) {
            const auto mode = bestAvailableVfsMode();
            if (mode == Vfs::WindowsCfApi || Theme::instance()->enableExperimentalFeatures()) {
                ac = menu->addAction(tr("Enable virtual file support%1...").arg(mode == Vfs::WindowsCfApi ? QString() : tr(" (experimental)")));
                connect(ac, &QAction::triggered, this, &AccountSettings::slotEnableVfsCurrentFolder);
            }
        }
        menu->popup(QCursor::pos());
    }
}

void AccountSettings::slotFolderListClicked(const QModelIndex &indx)
{
    if (indx.data(FolderStatusDelegate::AddButton).toBool()) {
        // "Add Folder Sync Connection"
        QTreeView *tv = ui->_folderList;
        auto pos = tv->mapFromGlobal(QCursor::pos());
        QStyleOptionViewItem opt;
        opt.initFrom(tv);
        auto btnRect = tv->visualRect(indx);
        auto btnSize = tv->itemDelegate(indx)->sizeHint(opt, indx);
        auto actual = QStyle::visualRect(opt.direction, btnRect, QRect(btnRect.topLeft(), btnSize));
        if (!actual.contains(pos))
            return;

        if (indx.flags() & Qt::ItemIsEnabled) {
            slotAddFolder();
        } else {
            QToolTip::showText(
                QCursor::pos(),
                _model->data(indx, Qt::ToolTipRole).toString(),
                this);
        }
        return;
    }
    if (_model->classify(indx) == FolderStatusModel::RootFolder) {
        // tries to find if we clicked on the '...' button.
        QTreeView *tv = ui->_folderList;
        auto pos = tv->mapFromGlobal(QCursor::pos());
        if (FolderStatusDelegate::optionsButtonRect(tv->visualRect(indx), layoutDirection()).contains(pos)) {
            slotCustomContextMenuRequested(pos);
            return;
        }
        if (FolderStatusDelegate::errorsListRect(tv->visualRect(indx), indx).contains(pos)) {
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

    FolderWizard *folderWizard = new FolderWizard(_accountState->account(), ocApp()->gui()->settingsDialog());
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

    bool useVfs = folderWizard->property("useVirtualFiles").toBool();

    auto folder = FolderMan::instance()->addFolderFromWizard(_accountState,
        folderWizard->field(QLatin1String("sourceFolder")).toString(),
        folderWizard->property("targetPath").toString(),
        folderWizard->davUrl(),
        folderWizard->displayName(),
        useVfs);


    const auto selectiveSyncBlackList = folderWizard->property("selectiveSyncBlackList").toStringList();
    if (!selectiveSyncBlackList.isEmpty() && OC_ENSURE(folder && !useVfs)) {
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, selectiveSyncBlackList);

        // The user already accepted the selective sync dialog. everything is in the white list
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList,
            QStringList() << QLatin1String("/"));
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
                _model->removeRow(row);

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

    auto messageBox = new AskExperimentalVirtualFilesFeatureMessageBox(this);

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

            // Setting to Unspecified retains existing data.
            // Selective sync excluded folders become OnlineOnly.
            folder->setRootPinState(PinState::Unspecified);
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
    if (bestAvailableVfsMode() == Vfs::WindowsCfApi) {
        Q_EMIT messageBox->accepted();
    } else {
        messageBox->show();
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

            // Wipe pin states and selective sync db
            folder->setRootPinState(PinState::AlwaysLocal);
            folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, {});

            // Prevent issues with missing local files
            folder->slotNextSyncFullLocalDiscovery();

            FolderMan::instance()->scheduleFolder(folder);

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

void AccountSettings::slotSetCurrentFolderAvailability(PinState state)
{
    OC_ASSERT(state == PinState::OnlineOnly || state == PinState::AlwaysLocal);

    QPointer<Folder> folder = selectedFolder();
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    if (!selected.isValid() || !folder)
        return;

    // similar to socket api: sets pin state recursively and sync
    folder->setRootPinState(state);
    folder->scheduleThisFolderSoon();
}

void AccountSettings::showConnectionLabel(const QString &message, QStringList errors)
{
    const QString errStyle = QLatin1String("color:#ffffff; background-color:#bb4d4d;padding:5px;"
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
                    tr("The syncing operation is running.<br/>Do you want to terminate it?"),
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
        FolderMan::instance()->scheduleFolderNext(selectedFolder);
    }
}

void AccountSettings::slotOpenOC()
{
    if (_OCUrl.isValid())
        QDesktopServices::openUrl(_OCUrl);
}

void AccountSettings::slotUpdateQuota(qint64 total, qint64 used)
{
    if (total > 0) {
        ui->quotaProgressBar->setVisible(true);
        ui->quotaProgressBar->setEnabled(true);
        // workaround the label only accepting ints (which may be only 32 bit wide)
        const double percent = used / (double)total * 100;
        const int percentInt = qMin(qRound(percent), 100);
        ui->quotaProgressBar->setValue(percentInt);
        QString usedStr = Utility::octetsToString(used);
        QString totalStr = Utility::octetsToString(total);
        QString percentStr = Utility::compactFormatDouble(percent, 1);
        QString toolTip = tr("%1 (%3%) of %2 in use. Some folders, including network mounted or shared folders, might have different limits.").arg(usedStr, totalStr, percentStr);
        ui->quotaInfoLabel->setText(tr("%1 of %2 in use").arg(usedStr, totalStr));
        ui->quotaInfoLabel->setToolTip(toolTip);
        ui->quotaProgressBar->setToolTip(toolTip);
    } else {
        ui->quotaProgressBar->setVisible(false);
        ui->quotaInfoLabel->setToolTip(QString());

        /* -1 means not computed; -2 means unknown; -3 means unlimited  (#3940)*/
        if (total == 0 || total == -1) {
            ui->quotaInfoLabel->setText(tr("Currently there is no storage usage information available."));
        } else {
            QString usedStr = Utility::octetsToString(used);
            ui->quotaInfoLabel->setText(tr("%1 in use").arg(usedStr));
        }
    }
}

void AccountSettings::slotAccountStateChanged()
{
    const AccountState::State state = _accountState ? _accountState->state() : AccountState::Disconnected;
    if (state != AccountState::Disconnected) {
        AccountPtr account = _accountState->account();
        QUrl safeUrl(account->url());
        safeUrl.setPassword(QString()); // Remove the password from the URL to avoid showing it in the UI
        FolderMan *folderMan = FolderMan::instance();
        for (auto *folder : folderMan->folders()) {
            _model->slotUpdateFolderState(folder);
        }

        const QString server = QString::fromLatin1("<a href=\"%1\">%2</a>")
                                   .arg(Utility::escape(account->url().toString()),
                                       Utility::escape(safeUrl.toString()));
        QString serverWithUser = server;
        if (AbstractCredentials *cred = account->credentials()) {
            QString user = account->davDisplayName();
            if (user.isEmpty()) {
                user = cred->user();
            }
            serverWithUser = tr("%1 as <i>%2</i>").arg(server, Utility::escape(user));
        }

        switch (state) {
        case AccountState::Connected: {
            QStringList errors;
            if (account->serverVersionUnsupported()) {
                errors << tr("The server version %1 is unsupported! Proceed at your own risk.").arg(account->serverVersionString());
            }
            showConnectionLabel(tr("Connected to %1.").arg(serverWithUser), errors);
            ui->openBrowserButton->setVisible(false);
            break;
        }
        case AccountState::ServiceUnavailable:
            showConnectionLabel(tr("Server %1 is temporarily unavailable.").arg(server));
            break;
        case AccountState::MaintenanceMode:
            showConnectionLabel(tr("Server %1 is currently in maintenance mode.").arg(server));
            break;
        case AccountState::SignedOut:
            showConnectionLabel(tr("Signed out from %1.").arg(serverWithUser));
            break;
        case AccountState::AskingCredentials: {
            auto cred = qobject_cast<HttpCredentialsGui *>(account->credentials());
            if (cred && cred->isUsingOAuth()) {
                connect(cred, &HttpCredentialsGui::authorisationLinkChanged,
                    this, &AccountSettings::slotAccountStateChanged, Qt::UniqueConnection);
                showConnectionLabel(tr("Obtaining authorization from the browser."));
                ui->openBrowserButton->setVisible(true);
            } else {
                showConnectionLabel(tr("Connecting to %1...").arg(serverWithUser));
            }
            break;
        }
        case AccountState::NetworkError:
            showConnectionLabel(tr("No connection to %1 at %2.")
                                    .arg(Utility::escape(Theme::instance()->appNameGUI()), server),
                _accountState->connectionErrors());
            break;
        case AccountState::ConfigurationError:
            showConnectionLabel(tr("Server configuration error: %1 at %2.")
                                    .arg(Utility::escape(Theme::instance()->appNameGUI()), server),
                _accountState->connectionErrors());
            break;
        case AccountState::Disconnected:
            // we can't end up here as the whole block is ifdeffed
            Q_UNREACHABLE();
            break;
        }
    } else {
        // ownCloud is not yet configured.
        showConnectionLabel(tr("No %1 connection configured.")
                                .arg(Utility::escape(Theme::instance()->appNameGUI())));
    }

    /* Allow to expand the item if the account is connected. */
    ui->_folderList->setItemsExpandable(state == AccountState::Connected);

    if (state != AccountState::Connected) {
        /* check if there are expanded root items, if so, close them */
        int i;
        for (i = 0; i < _model->rowCount(); ++i) {
            if (ui->_folderList->isExpanded(_model->index(i)))
                ui->_folderList->setExpanded(_model->index(i), false);
        }
    }

    // Disabling expansion of folders might require hiding the selective
    // sync user interface buttons.
    refreshSelectiveSyncStatus();

    /* set the correct label for the Account toolbox button */
    if (_accountState) {
        if (_accountState->isSignedOut()) {
            _toggleSignInOutAction->setText(tr("Log in"));
        } else {
            _toggleSignInOutAction->setText(tr("Log out"));
        }
    }
}

void AccountSettings::slotLinkActivated(const QString &link)
{
    // Parse folder alias and filename from the link, calculate the index
    // and select it if it exists.
    const QStringList li = link.split(QLatin1String("?folder="));
    if (li.count() > 1) {
        QString myFolder = li[0];
        const QByteArray id = QUrl::fromPercentEncoding(li[1].toUtf8()).toUtf8();
        if (myFolder.endsWith(QLatin1Char('/')))
            myFolder.chop(1);

        // Make sure the folder itself is expanded
        Folder *folder = FolderMan::instance()->folder(id);
        if (folder) {
            QModelIndex folderIndx = _model->indexForPath(folder, QString());
            if (!ui->_folderList->isExpanded(folderIndx)) {
                ui->_folderList->setExpanded(folderIndx, true);
            }

            QModelIndex indx = _model->indexForPath(folder, myFolder);
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
            if (myFolder.endsWith('/')) {
                myFolder.chop(1);
            }
            QModelIndex theIndx = _model->indexForPath(folder, myFolder);
            if (theIndx.isValid()) {
                msg += QString::fromLatin1("<a href=\"%1?folder=%2\">%1</a>")
                           .arg(Utility::escape(myFolder), QUrl::toPercentEncoding(folder->id()));
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
        _quotaInfo.setActive(isVisible());
    }
    if (e->type() == QEvent::Show) {
        // Expand the folder automatically only if there's only one, see #4283
        // The 2 is 1 folder + 1 'add folder' button
        if (_model->rowCount() <= 2) {
            ui->_folderList->setExpanded(_model->index(0, 0), true);
        }
    }
    return QWidget::event(e);
}

} // namespace OCC

#include "accountsettings.moc"
