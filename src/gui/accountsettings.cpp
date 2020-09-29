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

#include "theme.h"
#include "folderman.h"
#include "folderwizard.h"
#include "folderstatusmodel.h"
#include "folderstatusdelegate.h"
#include "common/utility.h"
#include "application.h"
#include "configfile.h"
#include "account.h"
#include "accountstate.h"
#include "userinfo.h"
#include "accountmanager.h"
#include "owncloudsetupwizard.h"
#include "creds/abstractcredentials.h"
#include "creds/httpcredentialsgui.h"
#include "tooltipupdater.h"
#include "filesystem.h"
#include "encryptfolderjob.h"
#include "syncresult.h"
#include "ignorelisttablewidget.h"

#include <cmath>

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QAction>
#include <QVBoxLayout>
#include <QTreeView>
#include <QKeySequence>
#include <QIcon>
#include <QVariant>
#include <QJsonDocument>
#include <QToolTip>
#include <qstringlistmodel.h>
#include <qpropertyanimation.h>

#include "account.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcAccountSettings, "nextcloud.gui.account.settings", QtInfoMsg)

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
                && (FolderStatusDelegate::errorsListRect(folderList->visualRect(index)).contains(pos)
                    || FolderStatusDelegate::optionsButtonRect(folderList->visualRect(index),folderList->layoutDirection()).contains(pos))) {
                shape = Qt::PointingHandCursor;
            }
            folderList->setCursor(shape);
        }
        return QObject::eventFilter(watched, event);
    }
};

AccountSettings::AccountSettings(AccountState *accountState, QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::AccountSettings)
    , _wasDisabledBefore(false)
    , _accountState(accountState)
    , _userInfo(accountState, false, true)
    , _menuShown(false)
{
    _ui->setupUi(this);

    _model = new FolderStatusModel;
    _model->setAccountState(_accountState);
    _model->setParent(this);
    auto *delegate = new FolderStatusDelegate;
    delegate->setParent(this);

    // Connect styleChanged events to our widgets, so they can adapt (Dark-/Light-Mode switching)
    connect(this, &AccountSettings::styleChanged, delegate, &FolderStatusDelegate::slotStyleChanged);

    _ui->_folderList->header()->hide();
    _ui->_folderList->setItemDelegate(delegate);
    _ui->_folderList->setModel(_model);
#if defined(Q_OS_MAC)
    _ui->_folderList->setMinimumWidth(400);
#else
    _ui->_folderList->setMinimumWidth(300);
#endif
    new ToolTipUpdater(_ui->_folderList);

    auto mouseCursorChanger = new MouseCursorChanger(this);
    mouseCursorChanger->folderList = _ui->_folderList;
    mouseCursorChanger->model = _model;
    _ui->_folderList->setMouseTracking(true);
    _ui->_folderList->setAttribute(Qt::WA_Hover, true);
    _ui->_folderList->installEventFilter(mouseCursorChanger);

    connect(this, &AccountSettings::removeAccountFolders,
            AccountManager::instance(), &AccountManager::removeAccountFolders);
    connect(_ui->_folderList, &QWidget::customContextMenuRequested,
        this, &AccountSettings::slotCustomContextMenuRequested);
    connect(_ui->_folderList, &QAbstractItemView::clicked,
        this, &AccountSettings::slotFolderListClicked);
    connect(_ui->_folderList, &QTreeView::expanded, this, &AccountSettings::refreshSelectiveSyncStatus);
    connect(_ui->_folderList, &QTreeView::collapsed, this, &AccountSettings::refreshSelectiveSyncStatus);
    connect(_ui->selectiveSyncNotification, &QLabel::linkActivated,
        this, &AccountSettings::slotLinkActivated);
    connect(_model, &FolderStatusModel::suggestExpand, _ui->_folderList, &QTreeView::expand);
    connect(_model, &FolderStatusModel::dirtyChanged, this, &AccountSettings::refreshSelectiveSyncStatus);
    refreshSelectiveSyncStatus();
    connect(_model, &QAbstractItemModel::rowsInserted,
        this, &AccountSettings::refreshSelectiveSyncStatus);

    auto *syncNowAction = new QAction(this);
    syncNowAction->setShortcut(QKeySequence(Qt::Key_F6));
    connect(syncNowAction, &QAction::triggered, this, &AccountSettings::slotScheduleCurrentFolder);
    addAction(syncNowAction);

    auto *syncNowWithRemoteDiscovery = new QAction(this);
    syncNowWithRemoteDiscovery->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_F6));
    connect(syncNowWithRemoteDiscovery, &QAction::triggered, this, &AccountSettings::slotScheduleCurrentFolderForceRemoteDiscovery);
    addAction(syncNowWithRemoteDiscovery);


    connect(_ui->selectiveSyncApply, &QAbstractButton::clicked, _model, &FolderStatusModel::slotApplySelectiveSync);
    connect(_ui->selectiveSyncCancel, &QAbstractButton::clicked, _model, &FolderStatusModel::resetFolders);
    connect(_ui->bigFolderApply, &QAbstractButton::clicked, _model, &FolderStatusModel::slotApplySelectiveSync);
    connect(_ui->bigFolderSyncAll, &QAbstractButton::clicked, _model, &FolderStatusModel::slotSyncAllPendingBigFolders);
    connect(_ui->bigFolderSyncNone, &QAbstractButton::clicked, _model, &FolderStatusModel::slotSyncNoPendingBigFolders);

    connect(FolderMan::instance(), &FolderMan::folderListChanged, _model, &FolderStatusModel::resetFolders);
    connect(this, &AccountSettings::folderChanged, _model, &FolderStatusModel::resetFolders);


    // quotaProgressBar style now set in customizeStyle()
    /*QColor color = palette().highlight().color();
     _ui->quotaProgressBar->setStyleSheet(QString::fromLatin1(progressBarStyleC).arg(color.name()));*/

    _ui->connectLabel->setText(tr("No account configured."));

    connect(_accountState, &AccountState::stateChanged, this, &AccountSettings::slotAccountStateChanged);
    slotAccountStateChanged();

    connect(&_userInfo, &UserInfo::quotaUpdated,
        this, &AccountSettings::slotUpdateQuota);

    // Connect E2E stuff
    connect(this, &AccountSettings::requesetMnemonic, _accountState->account()->e2e(), &ClientSideEncryption::slotRequestMnemonic);
    connect(_accountState->account()->e2e(), &ClientSideEncryption::showMnemonic, this, &AccountSettings::slotShowMnemonic);

    connect(_accountState->account()->e2e(), &ClientSideEncryption::mnemonicGenerated, this, &AccountSettings::slotNewMnemonicGenerated);
    if (_accountState->account()->e2e()->newMnemonicGenerated()) {
        slotNewMnemonicGenerated();
    } else {
        _ui->encryptionMessage->setText(tr("This account supports end-to-end encryption"));

        auto *mnemonic = new QAction(tr("Display mnemonic"), this);
        connect(mnemonic, &QAction::triggered, this, &AccountSettings::requesetMnemonic);
        _ui->encryptionMessage->addAction(mnemonic);
        _ui->encryptionMessage->hide();
    }

    connect(UserModel::instance(), &UserModel::addAccount,
         this, &AccountSettings::slotOpenAccountWizard);

    customizeStyle();
}


void AccountSettings::slotNewMnemonicGenerated()
{
    _ui->encryptionMessage->setText(tr("This account supports end-to-end encryption"));

    auto *mnemonic = new QAction(tr("Enable encryption"), this);
    connect(mnemonic, &QAction::triggered, this, &AccountSettings::requesetMnemonic);
    connect(mnemonic, &QAction::triggered, _ui->encryptionMessage, &KMessageWidget::hide);

    _ui->encryptionMessage->addAction(mnemonic);
    _ui->encryptionMessage->show();
}

void AccountSettings::slotEncryptFolderFinished(int status)
{
    qCInfo(lcAccountSettings) << "Current folder encryption status code:" << status;
    auto job = qobject_cast<EncryptFolderJob*>(sender());
    Q_ASSERT(job);
    if (!job->errorString().isEmpty()) {
        QMessageBox::warning(nullptr, tr("Warning"), job->errorString());
    }
    job->deleteLater();
}

QString AccountSettings::selectedFolderAlias() const
{
    QModelIndex selected = _ui->_folderList->selectionModel()->currentIndex();
    if (!selected.isValid())
        return "";
    return _model->data(selected, FolderStatusDelegate::FolderAliasRole).toString();
}

void AccountSettings::slotOpenAccountWizard()
{
    // We can't call isSystemTrayAvailable with appmenu-qt5 because it breaks the systemtray
    // (issue #4693, #4944)
    if (qgetenv("QT_QPA_PLATFORMTHEME") == "appmenu-qt5" || QSystemTrayIcon::isSystemTrayAvailable()) {
        topLevelWidget()->close();
    }

    OwncloudSetupWizard::runWizard(qApp, SLOT(slotownCloudWizardDone(int)), nullptr);
}

void AccountSettings::slotToggleSignInState()
{
    if (_accountState->isSignedOut()) {
        _accountState->account()->resetRejectedCertificates();
        _accountState->signIn();
    } else {
        _accountState->signOutByUi();
    }
}

void AccountSettings::doExpand()
{
    _ui->_folderList->expandToDepth(0);
}

void AccountSettings::slotShowMnemonic(const QString &mnemonic) {
    AccountManager::instance()->displayMnemonic(mnemonic);
}

bool AccountSettings::canEncryptOrDecrypt (const FolderStatusModel::SubFolderInfo* info) {
    if (info->_folder->syncResult().status() != SyncResult::Status::Success) {
        QMessageBox msgBox;
        msgBox.setText("Please wait for the folder to sync before trying to encrypt it.");
        msgBox.exec();
        return false;
    }

    // for some reason the actual folder in disk is info->_folder->path + info->_path.
    QDir folderPath(info->_folder->path() + info->_path);
    folderPath.setFilter( QDir::AllEntries | QDir::NoDotAndDotDot );

    if (folderPath.count() != 0) {
        QMessageBox msgBox;
        msgBox.setText(tr("You cannot encrypt a folder with contents, please remove the files.\n"
                       "Wait for the new sync, then encrypt it."));
        msgBox.exec();
        return false;
    }
    return true;
}

void AccountSettings::slotMarkSubfolderEncrypted(const FolderStatusModel::SubFolderInfo* folderInfo)
{
    if (!canEncryptOrDecrypt(folderInfo)) {
        return;
    }

    auto job = new OCC::EncryptFolderJob(accountsState()->account(), folderInfo->_path, folderInfo->_fileId, this);
    connect(job, &OCC::EncryptFolderJob::finished, this, &AccountSettings::slotEncryptFolderFinished);
    job->start();
}

void AccountSettings::slotEditCurrentIgnoredFiles()
{
    Folder *f = FolderMan::instance()->folder(selectedFolderAlias());
    if (!f)
        return;
    openIgnoredFilesDialog(f->path());
}

void AccountSettings::slotEditCurrentLocalIgnoredFiles()
{
    QModelIndex selected = _ui->_folderList->selectionModel()->currentIndex();
    if (!selected.isValid() || _model->classify(selected) != FolderStatusModel::SubFolder)
        return;
    QString fileName = _model->data(selected, FolderStatusDelegate::FolderPathRole).toString();
    openIgnoredFilesDialog(fileName);
}

void AccountSettings::openIgnoredFilesDialog(const QString & absFolderPath)
{
    Q_ASSERT(absFolderPath.startsWith('/'));
    Q_ASSERT(absFolderPath.endsWith('/'));

    const QString ignoreFile = absFolderPath + ".sync-exclude.lst";
    auto layout = new QVBoxLayout();
    auto ignoreListWidget = new IgnoreListTableWidget(this);
    ignoreListWidget->readIgnoreFile(ignoreFile);
    layout->addWidget(ignoreListWidget);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);

    auto dialog = new QDialog();
    dialog->setLayout(layout);

    connect(buttonBox, &QDialogButtonBox::clicked, [=](QAbstractButton * button) {
        if (buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole)
            ignoreListWidget->slotWriteIgnoreFile(ignoreFile);
        dialog->close();
    });
    connect(buttonBox, &QDialogButtonBox::rejected,
            dialog,    &QDialog::close);

    dialog->open();
}

void AccountSettings::slotSubfolderContextMenuRequested(const QModelIndex& index, const QPoint& pos)
{
    Q_UNUSED(pos);

    QMenu menu;
    auto ac = menu.addAction(tr("Open folder"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotOpenCurrentLocalSubFolder);

    auto fileName = _model->data(index, FolderStatusDelegate::FolderPathRole).toString();
    if (!QFile::exists(fileName)) {
        ac->setEnabled(false);
    }
    auto info   = _model->infoForIndex(index);
    auto acc = _accountState->account();

    if (acc->capabilities().clientSideEncryptionAvailable()) {
        // Verify if the folder is empty before attempting to encrypt.

        bool isEncrypted = acc->e2e()->isFolderEncrypted(info->_path);
        bool isParentEncrypted = acc->e2e()->isAnyParentFolderEncrypted(info->_path);

        if (!isEncrypted && !isParentEncrypted) {
            ac = menu.addAction(tr("Encrypt"));
            connect(ac, &QAction::triggered, [this, info] { slotMarkSubfolderEncrypted(info); });
        } else {
            // Ingore decrypting for now since it only works with an empty folder
            // connect(ac, &QAction::triggered, [this, &info] { slotMarkSubfolderDecrypted(info); });
        }
    }

    ac = menu.addAction(tr("Edit Ignored Files"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotEditCurrentLocalIgnoredFiles);

    menu.exec(QCursor::pos());
}

void AccountSettings::slotCustomContextMenuRequested(const QPoint &pos)
{
    QTreeView *tv = _ui->_folderList;
    QModelIndex index = tv->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    if (_model->classify(index) == FolderStatusModel::SubFolder) {
        slotSubfolderContextMenuRequested(index, pos);
        return;
    }

    if (_model->classify(index) != FolderStatusModel::RootFolder) {
        return;
    }

    tv->setCurrentIndex(index);
    QString alias = _model->data(index, FolderStatusDelegate::FolderAliasRole).toString();
    bool folderPaused = _model->data(index, FolderStatusDelegate::FolderSyncPaused).toBool();
    bool folderConnected = _model->data(index, FolderStatusDelegate::FolderAccountConnected).toBool();
    auto folderMan = FolderMan::instance();

    auto *menu = new QMenu(tv);

    menu->setAttribute(Qt::WA_DeleteOnClose);

    QAction *ac = menu->addAction(tr("Open folder"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotOpenCurrentFolder);

    ac = menu->addAction(tr("Edit Ignored Files"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotEditCurrentIgnoredFiles);

    if (!_ui->_folderList->isExpanded(index)) {
        ac = menu->addAction(tr("Choose what to sync"));
        ac->setEnabled(folderConnected);
        connect(ac, &QAction::triggered, this, &AccountSettings::doExpand);
    }

    if (!folderPaused) {
        ac = menu->addAction(tr("Force sync now"));
        if (folderMan->currentSyncFolder() == folderMan->folder(alias)) {
            ac->setText(tr("Restart sync"));
        }
        ac->setEnabled(folderConnected);
        connect(ac, &QAction::triggered, this, &AccountSettings::slotForceSyncCurrentFolder);
    }

    ac = menu->addAction(folderPaused ? tr("Resume sync") : tr("Pause sync"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotEnableCurrentFolder);

    ac = menu->addAction(tr("Remove folder sync connection"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotRemoveCurrentFolder);
    menu->popup(tv->mapToGlobal(pos));
}

void AccountSettings::slotFolderListClicked(const QModelIndex &indx)
{
    if (indx.data(FolderStatusDelegate::AddButton).toBool()) {
        // "Add Folder Sync Connection"
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
        QTreeView *tv = _ui->_folderList;
        auto pos = tv->mapFromGlobal(QCursor::pos());
        if (FolderStatusDelegate::optionsButtonRect(tv->visualRect(indx), layoutDirection()).contains(pos)) {
            slotCustomContextMenuRequested(pos);
            return;
        }
        if (FolderStatusDelegate::errorsListRect(tv->visualRect(indx)).contains(pos)) {
            emit showIssuesList(_accountState);
            return;
        }

        // Expand root items on single click
        if (_accountState && _accountState->state() == AccountState::Connected) {
            bool expanded = !(_ui->_folderList->isExpanded(indx));
            _ui->_folderList->setExpanded(indx, expanded);
        }
    }
}

void AccountSettings::slotAddFolder()
{
    FolderMan *folderMan = FolderMan::instance();
    folderMan->setSyncEnabled(false); // do not start more syncs.

    auto *folderWizard = new FolderWizard(_accountState->account(), this);

    connect(folderWizard, &QDialog::accepted, this, &AccountSettings::slotFolderWizardAccepted);
    connect(folderWizard, &QDialog::rejected, this, &AccountSettings::slotFolderWizardRejected);
    folderWizard->open();
}


void AccountSettings::slotFolderWizardAccepted()
{
    auto *folderWizard = qobject_cast<FolderWizard *>(sender());
    FolderMan *folderMan = FolderMan::instance();

    qCInfo(lcAccountSettings) << "Folder wizard completed";

    FolderDefinition definition;
    definition.localPath = FolderDefinition::prepareLocalPath(
        folderWizard->field(QLatin1String("sourceFolder")).toString());
    definition.targetPath = FolderDefinition::prepareTargetPath(
        folderWizard->property("targetPath").toString());

    {
        QDir dir(definition.localPath);
        if (!dir.exists()) {
            qCInfo(lcAccountSettings) << "Creating folder" << definition.localPath;
            if (!dir.mkpath(".")) {
                QMessageBox::warning(this, tr("Folder creation failed"),
                    tr("<p>Could not create local folder <i>%1</i>.</p>")
                        .arg(QDir::toNativeSeparators(definition.localPath)));
                return;
            }
        }
        FileSystem::setFolderMinimumPermissions(definition.localPath);
        Utility::setupFavLink(definition.localPath);
    }

    /* take the value from the definition of already existing folders. All folders have
     * the same setting so far.
     * The default is to sync hidden files
     */
    definition.ignoreHiddenFiles = folderMan->ignoreHiddenFiles();

    if (folderMan->navigationPaneHelper().showInExplorerNavigationPane())
        definition.navigationPaneClsid = QUuid::createUuid();

    auto selectiveSyncBlackList = folderWizard->property("selectiveSyncBlackList").toStringList();

    folderMan->setSyncEnabled(true);

    Folder *f = folderMan->addFolder(_accountState, definition);
    if (f) {
        f->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, selectiveSyncBlackList);

        // The user already accepted the selective sync dialog. everything is in the white list
        f->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList,
            QStringList() << QLatin1String("/"));
        folderMan->scheduleAllFolders();
        emit folderChanged();
    }
}

void AccountSettings::slotFolderWizardRejected()
{
    qCInfo(lcAccountSettings) << "Folder wizard cancelled";
    FolderMan *folderMan = FolderMan::instance();
    folderMan->setSyncEnabled(true);
}

void AccountSettings::slotRemoveCurrentFolder()
{
    FolderMan *folderMan = FolderMan::instance();
    auto folder = folderMan->folder(selectedFolderAlias());
    QModelIndex selected = _ui->_folderList->selectionModel()->currentIndex();
    if (selected.isValid() && folder) {
        int row = selected.row();

        qCInfo(lcAccountSettings) << "Remove Folder alias " << folder->alias();
        QString shortGuiLocalPath = folder->shortGuiLocalPath();

        QMessageBox messageBox(QMessageBox::Question,
            tr("Confirm Folder Sync Connection Removal"),
            tr("<p>Do you really want to stop syncing the folder <i>%1</i>?</p>"
               "<p><b>Note:</b> This will <b>not</b> delete any files.</p>")
                .arg(shortGuiLocalPath),
            QMessageBox::NoButton,
            this);
        QPushButton *yesButton =
            messageBox.addButton(tr("Remove Folder Sync Connection"), QMessageBox::YesRole);
        messageBox.addButton(tr("Cancel"), QMessageBox::NoRole);

        messageBox.exec();
        if (messageBox.clickedButton() != yesButton) {
            return;
        }

        folderMan->removeFolder(folder);
        _model->removeRow(row);

        // single folder fix to show add-button and hide remove-button

        emit folderChanged();
    }
}

void AccountSettings::slotOpenCurrentFolder()
{
    auto alias = selectedFolderAlias();
    if (!alias.isEmpty()) {
        emit openFolderAlias(alias);
    }
}

void AccountSettings::slotOpenCurrentLocalSubFolder()
{
    QModelIndex selected = _ui->_folderList->selectionModel()->currentIndex();
    if (!selected.isValid() || _model->classify(selected) != FolderStatusModel::SubFolder)
        return;
    QString fileName = _model->data(selected, FolderStatusDelegate::FolderPathRole).toString();
    QUrl url = QUrl::fromLocalFile(fileName);
    QDesktopServices::openUrl(url);
}

void AccountSettings::showConnectionLabel(const QString &message, QStringList errors)
{
    const QString errStyle = QLatin1String("color:#ffffff; background-color:#bb4d4d;padding:5px;"
                                           "border-width: 1px; border-style: solid; border-color: #aaaaaa;"
                                           "border-radius:5px;");
    if (errors.isEmpty()) {
        QString msg = message;
        Theme::replaceLinkColorStringBackgroundAware(msg);
        _ui->connectLabel->setText(msg);
        _ui->connectLabel->setToolTip(QString());
        _ui->connectLabel->setStyleSheet(QString());
    } else {
        errors.prepend(message);
        QString msg = errors.join(QLatin1String("\n"));
        qCDebug(lcAccountSettings) << msg;
        Theme::replaceLinkColorString(msg, QColor("#c1c8e6"));
        _ui->connectLabel->setText(msg);
        _ui->connectLabel->setToolTip(QString());
        _ui->connectLabel->setStyleSheet(errStyle);
    }
    _ui->accountStatus->setVisible(!message.isEmpty());
}

void AccountSettings::slotEnableCurrentFolder()
{
    auto alias = selectedFolderAlias();

    if (!alias.isEmpty()) {
        FolderMan *folderMan = FolderMan::instance();

        qCInfo(lcAccountSettings) << "Application: enable folder with alias " << alias;
        bool terminate = false;
        bool currentlyPaused = false;

        // this sets the folder status to disabled but does not interrupt it.
        Folder *f = folderMan->folder(alias);
        if (!f) {
            return;
        }
        currentlyPaused = f->syncPaused();
        if (!currentlyPaused) {
            // check if a sync is still running and if so, ask if we should terminate.
            if (f->isBusy()) { // its still running
#if defined(Q_OS_MAC)
                QWidget *parent = this;
                Qt::WindowFlags flags = Qt::Sheet;
#else
                QWidget *parent = nullptr;
                Qt::WindowFlags flags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint; // default flags
#endif
                QMessageBox msgbox(QMessageBox::Question, tr("Sync Running"),
                    tr("The syncing operation is running.<br/>Do you want to terminate it?"),
                    QMessageBox::Yes | QMessageBox::No, parent, flags);
                msgbox.setDefaultButton(QMessageBox::Yes);
                int reply = msgbox.exec();
                if (reply == QMessageBox::Yes)
                    terminate = true;
                else
                    return; // do nothing
            }
        }

        // message box can return at any time while the thread keeps running,
        // so better check again after the user has responded.
        if (f->isBusy() && terminate) {
            f->slotTerminateSync();
        }
        f->setSyncPaused(!currentlyPaused);

        // keep state for the icon setting.
        if (currentlyPaused)
            _wasDisabledBefore = true;

        _model->slotUpdateFolderState(f);
    }
}

void AccountSettings::slotScheduleCurrentFolder()
{
    FolderMan *folderMan = FolderMan::instance();
    if (auto folder = folderMan->folder(selectedFolderAlias())) {
        folderMan->scheduleFolder(folder);
    }
}

void AccountSettings::slotScheduleCurrentFolderForceRemoteDiscovery()
{
    FolderMan *folderMan = FolderMan::instance();
    if (auto folder = folderMan->folder(selectedFolderAlias())) {
        folder->journalDb()->forceRemoteDiscoveryNextSync();
        folderMan->scheduleFolder(folder);
    }
}

void AccountSettings::slotForceSyncCurrentFolder()
{
    FolderMan *folderMan = FolderMan::instance();
    if (auto selectedFolder = folderMan->folder(selectedFolderAlias())) {
        // Terminate and reschedule any running sync
        if (Folder *current = folderMan->currentSyncFolder()) {
            folderMan->terminateSyncProcess();
            folderMan->scheduleFolder(current);
        }

        // Insert the selected folder at the front of the queue
        folderMan->scheduleFolderNext(selectedFolder);
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
        _ui->quotaProgressBar->setVisible(true);
        _ui->quotaProgressBar->setEnabled(true);
        // workaround the label only accepting ints (which may be only 32 bit wide)
        const double percent = used / (double)total * 100;
        const int percentInt = qMin(qRound(percent), 100);
        _ui->quotaProgressBar->setValue(percentInt);
        QString usedStr = Utility::octetsToString(used);
        QString totalStr = Utility::octetsToString(total);
        QString percentStr = Utility::compactFormatDouble(percent, 1);
        QString toolTip = tr("%1 (%3%) of %2 in use. Some folders, including network mounted or shared folders, might have different limits.").arg(usedStr, totalStr, percentStr);
        _ui->quotaInfoLabel->setText(tr("%1 of %2 in use").arg(usedStr, totalStr));
        _ui->quotaInfoLabel->setToolTip(toolTip);
        _ui->quotaProgressBar->setToolTip(toolTip);
    } else {
        _ui->quotaProgressBar->setVisible(false);
        _ui->quotaInfoLabel->setToolTip(QString());

        /* -1 means not computed; -2 means unknown; -3 means unlimited  (#owncloud/client/issues/3940)*/
        if (total == 0 || total == -1) {
            _ui->quotaInfoLabel->setText(tr("Currently there is no storage usage information available."));
        } else {
            QString usedStr = Utility::octetsToString(used);
            _ui->quotaInfoLabel->setText(tr("%1 in use").arg(usedStr));
        }
    }
}

void AccountSettings::slotAccountStateChanged()
{
    int state = _accountState ? _accountState->state() : AccountState::Disconnected;
    if (_accountState) {
        _ui->sslButton->updateAccountState(_accountState);
        AccountPtr account = _accountState->account();
        QUrl safeUrl(account->url());
        safeUrl.setPassword(QString()); // Remove the password from the URL to avoid showing it in the UI
        const auto folders = FolderMan::instance()->map().values();
        for (Folder *folder : folders) {
            _model->slotUpdateFolderState(folder);
        }

        QString server = QString::fromLatin1("<a href=\"%1\">%2</a>")
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

        if (state == AccountState::Connected) {
            QStringList errors;
            if (account->serverVersionUnsupported()) {
                errors << tr("The server version %1 is old and unsupported! Proceed at your own risk.").arg(account->serverVersion());
            }
            showConnectionLabel(tr("Connected to %1.").arg(serverWithUser), errors);
        } else if (state == AccountState::ServiceUnavailable) {
            showConnectionLabel(tr("Server %1 is temporarily unavailable.").arg(server));
        } else if (state == AccountState::MaintenanceMode) {
            showConnectionLabel(tr("Server %1 is currently in maintenance mode.").arg(server));
        } else if (state == AccountState::SignedOut) {
            showConnectionLabel(tr("Signed out from %1.").arg(serverWithUser));
        } else if (state == AccountState::AskingCredentials) {
            QUrl url;
            if (auto cred = qobject_cast<HttpCredentialsGui *>(account->credentials())) {
                connect(cred, &HttpCredentialsGui::authorisationLinkChanged,
                    this, &AccountSettings::slotAccountStateChanged, Qt::UniqueConnection);
                url = cred->authorisationLink();
            }
            if (url.isValid()) {
                showConnectionLabel(tr("Obtaining authorization from the browser. "
                                       "<a href='%1'>Click here</a> to re-open the browser.")
                                        .arg(url.toString(QUrl::FullyEncoded)));
            } else {
                showConnectionLabel(tr("Connecting to %1 …").arg(serverWithUser));
            }
        } else {
            showConnectionLabel(tr("No connection to %1 at %2.")
                                    .arg(Utility::escape(Theme::instance()->appNameGUI()), server),
                _accountState->connectionErrors());
        }
    } else {
        // ownCloud is not yet configured.
        showConnectionLabel(tr("No %1 connection configured.")
                                .arg(Utility::escape(Theme::instance()->appNameGUI())));
    }

    /* Allow to expand the item if the account is connected. */
    _ui->_folderList->setItemsExpandable(state == AccountState::Connected);

    if (state != AccountState::Connected) {
        /* check if there are expanded root items, if so, close them */
        int i = 0;
        for (i = 0; i < _model->rowCount(); ++i) {
            if (_ui->_folderList->isExpanded(_model->index(i)))
                _ui->_folderList->setExpanded(_model->index(i), false);
        }
    } else if (_model->isDirty()) {
        // If we connect and have pending changes, show the list.
        doExpand();
    }

    // Disabling expansion of folders might require hiding the selective
    // sync user interface buttons.
    refreshSelectiveSyncStatus();

    if (state == AccountState::State::Connected) {
        /* TODO: We should probably do something better here.
         * Verify if the user has a private key already uploaded to the server,
         * if it has, do not offer to create one.
         */
        qCInfo(lcAccountSettings) << "Account" << accountsState()->account()->displayName()
            << "Client Side Encryption" << accountsState()->account()->capabilities().clientSideEncryptionAvailable();

        if (_accountState->account()->capabilities().clientSideEncryptionAvailable()) {
            _ui->encryptionMessage->show();
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
        const QString alias = li[1];
        if (myFolder.endsWith(QLatin1Char('/')))
            myFolder.chop(1);

        // Make sure the folder itself is expanded
        Folder *f = FolderMan::instance()->folder(alias);
        QModelIndex folderIndx = _model->indexForPath(f, QString());
        if (!_ui->_folderList->isExpanded(folderIndx)) {
            _ui->_folderList->setExpanded(folderIndx, true);
        }

        QModelIndex indx = _model->indexForPath(f, myFolder);
        if (indx.isValid()) {
            // make sure all the parents are expanded
            for (auto i = indx.parent(); i.isValid(); i = i.parent()) {
                if (!_ui->_folderList->isExpanded(i)) {
                    _ui->_folderList->setExpanded(i, true);
                }
            }
            _ui->_folderList->setSelectionMode(QAbstractItemView::SingleSelection);
            _ui->_folderList->setCurrentIndex(indx);
            _ui->_folderList->scrollTo(indx);
        } else {
            qCWarning(lcAccountSettings) << "Unable to find a valid index for " << myFolder;
        }
    }
}

AccountSettings::~AccountSettings()
{
    delete _ui;
}

void AccountSettings::refreshSelectiveSyncStatus()
{
    bool shouldBeVisible = _model->isDirty() && _accountState->isConnected();

    QString msg;
    int cnt = 0;
    const auto folders = FolderMan::instance()->map().values();
    for (Folder *folder : folders) {
        if (folder->accountState() != _accountState) {
            continue;
        }

        bool ok = false;
        const auto undecidedList = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, &ok);
        QString p;
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
                           .arg(Utility::escape(myFolder), Utility::escape(folder->alias()));
            } else {
                msg += myFolder; // no link because we do not know the index yet.
            }
        }
    }

    if (msg.isEmpty()) {
        _ui->selectiveSyncButtons->setVisible(true);
        _ui->bigFolderUi->setVisible(false);
    } else {
        ConfigFile cfg;
        QString info = !cfg.confirmExternalStorage()
            ? tr("There are folders that were not synchronized because they are too big: ")
            : !cfg.newBigFolderSizeLimit().first
                ? tr("There are folders that were not synchronized because they are external storages: ")
                : tr("There are folders that were not synchronized because they are too big or external storages: ");

        _ui->selectiveSyncNotification->setText(info + msg);
        _ui->selectiveSyncButtons->setVisible(false);
        _ui->bigFolderUi->setVisible(true);
        shouldBeVisible = true;
    }

    _ui->selectiveSyncApply->setEnabled(_model->isDirty() || !msg.isEmpty());
    bool wasVisible = !_ui->selectiveSyncStatus->isHidden();
    if (wasVisible != shouldBeVisible) {
        QSize hint = _ui->selectiveSyncStatus->sizeHint();
        if (shouldBeVisible) {
            _ui->selectiveSyncStatus->setMaximumHeight(0);
            _ui->selectiveSyncStatus->setVisible(true);
        }
        auto anim = new QPropertyAnimation(_ui->selectiveSyncStatus, "maximumHeight", _ui->selectiveSyncStatus);
        anim->setEndValue(shouldBeVisible ? hint.height() : 0);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        connect(anim, &QPropertyAnimation::finished, [this, shouldBeVisible]() {
            _ui->selectiveSyncStatus->setMaximumHeight(QWIDGETSIZE_MAX);
            if (!shouldBeVisible) {
                _ui->selectiveSyncStatus->hide();
            }
        });
    }
}

void AccountSettings::slotDeleteAccount()
{
    // Deleting the account potentially deletes 'this', so
    // the QMessageBox should be destroyed before that happens.
    {
        QMessageBox messageBox(QMessageBox::Question,
            tr("Confirm Account Removal"),
            tr("<p>Do you really want to remove the connection to the account <i>%1</i>?</p>"
               "<p><b>Note:</b> This will <b>not</b> delete any files.</p>")
                .arg(_accountState->account()->displayName()),
            QMessageBox::NoButton,
            this);
        QPushButton *yesButton =
            messageBox.addButton(tr("Remove connection"), QMessageBox::YesRole);
        messageBox.addButton(tr("Cancel"), QMessageBox::NoRole);

        messageBox.exec();
        if (messageBox.clickedButton() != yesButton) {
            return;
        }
    }

    // Else it might access during destruction. This should be better handled by it having a QSharedPointer
    _model->setAccountState(nullptr);

    auto manager = AccountManager::instance();
    manager->deleteAccount(_accountState);
    manager->save();

    // IMPORTANT: "this" is deleted from this point on. We should probably remove this synchronous
    // .exec() QMessageBox magic above as it recurses into the event loop.
}

bool AccountSettings::event(QEvent *e)
{
    if (e->type() == QEvent::Hide || e->type() == QEvent::Show) {
        _userInfo.setActive(isVisible());
    }
    if (e->type() == QEvent::Show) {
        // Expand the folder automatically only if there's only one, see #4283
        // The 2 is 1 folder + 1 'add folder' button
        if (_model->rowCount() <= 2) {
            _ui->_folderList->setExpanded(_model->index(0, 0), true);
        }
    }
    return QWidget::event(e);
}

void AccountSettings::slotStyleChanged()
{
    customizeStyle();

    // Notify the other widgets (Dark-/Light-Mode switching)
    emit styleChanged();
}

void AccountSettings::customizeStyle()
{
    QString msg = _ui->connectLabel->text();
    Theme::replaceLinkColorStringBackgroundAware(msg);
    _ui->connectLabel->setText(msg);

    QColor color = palette().highlight().color();
    _ui->quotaProgressBar->setStyleSheet(QString::fromLatin1(progressBarStyleC).arg(color.name()));
}

} // namespace OCC

#include "accountsettings.moc"
