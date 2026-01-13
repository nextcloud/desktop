/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "accountsettings.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "qmessagebox.h"
#include "ui_accountsettings.h"

#include "theme.h"
#include "foldercreationdialog.h"
#include "folderman.h"
#include "folderwizard.h"
#include "folderstatusmodel.h"
#include "folderstatusdelegate.h"
#include "common/utility.h"
#include "guiutility.h"
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
#include "wizard/owncloudwizard.h"
#include "networksettings.h"
#include "ui_mnemonicdialog.h"

#include <cmath>

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QAction>
#include <QAbstractScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QTreeView>
#include <QKeySequence>
#include <QIcon>
#include <QVariant>
#include <QJsonDocument>
#include <QToolTip>
#include <QPushButton>
#include <QStyle>

#ifdef BUILD_FILE_PROVIDER_MODULE
#include "macOS/fileprovider.h"
#endif

#include "account.h"

namespace {
constexpr auto propertyFolder = "folder";
constexpr auto propertyPath = "path";
constexpr auto e2eUiActionIdKey = "id";
constexpr auto e2EeUiActionSetupEncryptionId = "setup_encryption";
constexpr auto e2EeUiActionForgetEncryptionId = "forget_encryption";
constexpr auto e2EeUiActionDisplayMnemonicId = "display_mnemonic";
constexpr auto e2EeUiActionMigrateCertificateId = "migrate_certificate";
}

namespace OCC {

class AccountSettings;

Q_LOGGING_CATEGORY(lcAccountSettings, "nextcloud.gui.account.settings", QtInfoMsg)

void showEnableE2eeWithVirtualFilesWarningDialog(std::function<void(void)> onAccept)
{
    const auto messageBox = new QMessageBox;
    messageBox->setAttribute(Qt::WA_DeleteOnClose);
    messageBox->setText(AccountSettings::tr("End-to-end Encryption with Virtual Files"));
    messageBox->setInformativeText(AccountSettings::tr("You seem to have the Virtual Files feature enabled on this folder. "
                                                       "At the moment, it is not possible to implicitly download virtual files that are "
                                                       "end-to-end encrypted. To get the best experience with virtual files and "
                                                       "end-to-end encryption, make sure the encrypted folder is marked with "
                                                       "\"Make always available locally\"."));
    messageBox->setIcon(QMessageBox::Warning);
    const auto dontEncryptButton = messageBox->addButton(QMessageBox::StandardButton::Cancel);
    Q_ASSERT(dontEncryptButton);
    dontEncryptButton->setText(AccountSettings::tr("Do not encrypt folder"));
    const auto encryptButton = messageBox->addButton(QMessageBox::StandardButton::Ok);
    Q_ASSERT(encryptButton);
    encryptButton->setText(AccountSettings::tr("Encrypt folder"));
    QObject::connect(messageBox, &QMessageBox::accepted, onAccept);

    messageBox->open();
}

void showEnableE2eeWarningDialog(std::function<void(void)> onAccept)
{
    const auto messageBox = new QMessageBox;
    messageBox->setAttribute(Qt::WA_DeleteOnClose);
    messageBox->setText(AccountSettings::tr("End-to-end Encryption"));
    messageBox->setTextFormat(Qt::RichText);
    messageBox->setInformativeText(
        AccountSettings::tr("This will encrypt your folder and all files within it. "
                            "These files will no longer be accessible without your encryption mnemonic key. "
                            "\n<b>This process is not reversible. Are you sure you want to proceed?</b>"));
    messageBox->setIcon(QMessageBox::Warning);
    const auto dontEncryptButton = messageBox->addButton(QMessageBox::StandardButton::Cancel);
    Q_ASSERT(dontEncryptButton);
    dontEncryptButton->setText(AccountSettings::tr("Do not encrypt folder"));
    const auto encryptButton = messageBox->addButton(QMessageBox::StandardButton::Ok);
    Q_ASSERT(encryptButton);
    encryptButton->setText(AccountSettings::tr("Encrypt folder"));
    QObject::connect(messageBox, &QMessageBox::accepted, onAccept);

    messageBox->open();
}

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

    QTreeView *folderList = nullptr;
    FolderStatusModel *model = nullptr;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::HoverMove) {
            Qt::CursorShape shape = Qt::ArrowCursor;
            const auto pos = folderList->mapFromGlobal(QCursor::pos());
            const auto index = folderList->indexAt(pos);
            if (model->classify(index) == FolderStatusModel::RootFolder &&
                (FolderStatusDelegate::errorsListRect(folderList->visualRect(index)).contains(pos) ||
                    FolderStatusDelegate::optionsButtonRect(folderList->visualRect(index),folderList->layoutDirection()).contains(pos))) {
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
    , _model(new FolderStatusModel)
    , _accountState(accountState)
    , _userInfo(accountState, false, true)
{
    _ui->setupUi(this);
    _ui->gridLayout->setRowStretch(0, 0);
    _ui->gridLayout->setRowStretch(1, 0);
    _ui->gridLayout->setRowStretch(2, 0);
    _ui->gridLayout->setRowStretch(3, 0);
    _ui->gridLayout->setRowStretch(4, 1);

    _model->setAccountState(_accountState);
    _model->setParent(this);
    const auto delegate = new FolderStatusDelegate;
    delegate->setParent(this);

    _ui->accountTabsPanel->setStyleSheet(QStringLiteral(
        "QWidget#syncFoldersPanelContents, QWidget#connectionSettingsPanelContents, QWidget#fileProviderPanelContents {"
        " background: palette(alternate-base); }"));
    _ui->syncFoldersPanelContents->setAutoFillBackground(true);
    _ui->syncFoldersPanelContents->setAttribute(Qt::WA_StyledBackground, true);
    _ui->syncFoldersPanelContents->setContentsMargins(0, 0, 0, 0);
    _ui->fileProviderPanelContents->setAutoFillBackground(true);
    _ui->fileProviderPanelContents->setAttribute(Qt::WA_StyledBackground, true);
    _ui->fileProviderPanelContents->setContentsMargins(0, 0, 0, 0);
    _ui->connectionSettingsPanelContents->setAutoFillBackground(true);
    _ui->connectionSettingsPanelContents->setAttribute(Qt::WA_StyledBackground, true);
    _ui->connectionSettingsPanelContents->setContentsMargins(0, 0, 0, 0);

    // Connect styleChanged events to our widgets, so they can adapt (Dark-/Light-Mode switching)
    connect(this, &AccountSettings::styleChanged, delegate, &FolderStatusDelegate::slotStyleChanged);

    _ui->_folderList->header()->hide();
    _ui->_folderList->setAutoFillBackground(true);
    _ui->_folderList->setAttribute(Qt::WA_StyledBackground, true);
    _ui->_folderList->setStyleSheet(QStringLiteral("QTreeView { background: palette(alternate-base); }"));
    _ui->_folderList->setItemDelegate(delegate);
    _ui->_folderList->setModel(_model);
    _ui->_folderList->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    _ui->_folderList->setMinimumWidth(300);

    new ToolTipUpdater(_ui->_folderList);

#if defined(BUILD_FILE_PROVIDER_MODULE)
    const auto fileProviderPanelContents = _ui->fileProviderPanelContents;
    const auto fpSettingsLayout = new QVBoxLayout(fileProviderPanelContents);
    const auto fpAccountUserIdAtHost = _accountState->account()->userIdAtHostWithPort();
    const auto fpSettingsController = Mac::FileProviderSettingsController::instance();
    const auto fpSettingsWidget = fpSettingsController->settingsViewWidget(fpAccountUserIdAtHost, fileProviderPanelContents,
                                                                           QQuickWidget::SizeRootObjectToView);
    fpSettingsLayout->setContentsMargins(0, 0, 0, 0);
    fpSettingsLayout->setSpacing(0);
    
    fpSettingsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (const auto fpSettingsWidgetLayout = fpSettingsWidget->layout()) {
        fpSettingsWidgetLayout->setContentsMargins(0, 0, 0, 0);
    }
    fpSettingsLayout->addWidget(fpSettingsWidget, 1);
    fileProviderPanelContents->setLayout(fpSettingsLayout);
#else
    _ui->fileProviderPanel->setVisible(false);
#endif

    const auto connectionSettingsPanelContents = _ui->connectionSettingsPanelContents;
    const auto connectionSettingsLayout = new QVBoxLayout(connectionSettingsPanelContents);
    const auto networkSettings = new NetworkSettings(_accountState->account(), connectionSettingsPanelContents);
    if (const auto networkSettingsLayout = networkSettings->layout()) {
        networkSettingsLayout->setContentsMargins(0, 0, 0, 0);
    }
    connectionSettingsLayout->setContentsMargins(0, 0, 0, 0);
    connectionSettingsLayout->setSpacing(0);
    connectionSettingsLayout->addWidget(networkSettings, 1);
    connectionSettingsPanelContents->setLayout(connectionSettingsLayout);
    
    const auto mouseCursorChanger = new MouseCursorChanger(this);
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
    connect(_model, &QAbstractItemModel::rowsInserted,
        _ui->_folderList, &QWidget::updateGeometry);
    connect(_model, &QAbstractItemModel::rowsRemoved,
        _ui->_folderList, &QWidget::updateGeometry);
    connect(_model, &QAbstractItemModel::modelReset,
        _ui->_folderList, &QWidget::updateGeometry);

    auto *syncNowAction = new QAction(this);
    syncNowAction->setShortcut(QKeySequence(Qt::Key_F6));
    connect(syncNowAction, &QAction::triggered, this, &AccountSettings::slotScheduleCurrentFolder);
    addAction(syncNowAction);

    auto *syncNowWithRemoteDiscovery = new QAction(this);
    syncNowWithRemoteDiscovery->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F6));
    connect(syncNowWithRemoteDiscovery, &QAction::triggered, this, &AccountSettings::slotScheduleCurrentFolderForceRemoteDiscovery);
    addAction(syncNowWithRemoteDiscovery);


    slotHideSelectiveSyncWidget();
    _ui->bigFolderUi->setVisible(false);
    connect(_model, &QAbstractItemModel::dataChanged, this, &AccountSettings::slotSelectiveSyncChanged);
    connect(_ui->selectiveSyncApply, &QAbstractButton::clicked, this, &AccountSettings::slotHideSelectiveSyncWidget);
    connect(_ui->selectiveSyncCancel, &QAbstractButton::clicked, this, &AccountSettings::slotHideSelectiveSyncWidget);

    connect(_ui->selectiveSyncApply, &QAbstractButton::clicked, _model, &FolderStatusModel::slotApplySelectiveSync);
    connect(_ui->selectiveSyncCancel, &QAbstractButton::clicked, _model, &FolderStatusModel::resetFolders);
    connect(_ui->bigFolderApply, &QAbstractButton::clicked, _model, &FolderStatusModel::slotApplySelectiveSync);
    connect(_ui->bigFolderSyncAll, &QAbstractButton::clicked, _model, &FolderStatusModel::slotSyncAllPendingBigFolders);
    connect(_ui->bigFolderSyncNone, &QAbstractButton::clicked, _model, &FolderStatusModel::slotSyncNoPendingBigFolders);

    connect(FolderMan::instance(), &FolderMan::folderListChanged, _model, &FolderStatusModel::resetFolders);
    connect(this, &AccountSettings::folderChanged, _model, &FolderStatusModel::resetFolders);

    // Connect E2E stuff
    if (_accountState->isConnected()) {
        setupE2eEncryption();
    } else {
        _ui->encryptionMessageLabel->setText(tr("End-to-end encryption has not been initialized on this account."));
    }
    _ui->encryptionMessageLabel->setTextFormat(Qt::RichText);
    _ui->encryptionMessageLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    _ui->encryptionMessageLabel->setOpenExternalLinks(true);
    _ui->encryptionMessageButtonsLayout->addStretch();
    setEncryptionMessageIcon({});

    _ui->connectLabel->setText(tr("No account configured."));

    connect(_accountState, &AccountState::stateChanged, this, &AccountSettings::slotAccountStateChanged);
    slotAccountStateChanged();

    connect(&_userInfo, &UserInfo::quotaUpdated,
        this, &AccountSettings::slotUpdateQuota);

    customizeStyle();

    connect(_accountState->account()->e2e(), &ClientSideEncryption::startingDiscoveryEncryptionUsbToken,
            Systray::instance(), &Systray::createEncryptionTokenDiscoveryDialog);
    connect(_accountState->account()->e2e(), &ClientSideEncryption::finishedDiscoveryEncryptionUsbToken,
            Systray::instance(), &Systray::destroyEncryptionTokenDiscoveryDialog);
}

void AccountSettings::slotE2eEncryptionMnemonicReady()
{
    const auto actionDisableEncryption = addActionToEncryptionMessage(tr("Forget encryption setup"), e2EeUiActionForgetEncryptionId);
    connect(actionDisableEncryption, &QAction::triggered, this, [this] {
        forgetEncryptionOnDeviceForAccount(_accountState->account());
    });

    if (_accountState->account()->e2e()->userCertificateNeedsMigration()) {
        slotE2eEncryptionCertificateNeedMigration();
    }

    if (!_accountState->account()->e2e()->getMnemonic().isEmpty()) {
        const auto actionDisplayMnemonic = addActionToEncryptionMessage(tr("Display mnemonic"), e2EeUiActionDisplayMnemonicId);
        connect(actionDisplayMnemonic, &QAction::triggered, this, [this]() {
            displayMnemonic(_accountState->account()->e2e()->getMnemonic());
        });
    }

    _ui->encryptionMessageLabel->setText(tr("Encryption is set-up. Remember to <b>Encrypt</b> a folder to end-to-end encrypt any new files added to it."));
    setEncryptionMessageIcon(Theme::createColorAwareIcon(QStringLiteral(":/client/theme/lock.svg")));
    _ui->encryptionMessage->show();
}

void AccountSettings::slotE2eEncryptionGenerateKeys()
{
    connect(_accountState->account()->e2e(), &ClientSideEncryption::initializationFinished, this, &AccountSettings::slotE2eEncryptionInitializationFinished);
    _accountState->account()->setE2eEncryptionKeysGenerationAllowed(true);
    _accountState->account()->setAskUserForMnemonic(true);
    _accountState->account()->e2e()->initialize(this);
}

void AccountSettings::slotE2eEncryptionInitializationFinished(bool isNewMnemonicGenerated)
{
    disconnect(_accountState->account()->e2e(), &ClientSideEncryption::initializationFinished, this, &AccountSettings::slotE2eEncryptionInitializationFinished);
    if (_accountState->account()->e2e()->isInitialized()) {
        removeActionFromEncryptionMessage(e2EeUiActionSetupEncryptionId);
        slotE2eEncryptionMnemonicReady();
        if (isNewMnemonicGenerated) {
            displayMnemonic(_accountState->account()->e2e()->getMnemonic());
        }
        Q_EMIT _accountState->account()->wantsFoldersSynced();
    }
    _accountState->account()->setAskUserForMnemonic(false);
}

void AccountSettings::slotEncryptFolderFinished(int status)
{
    qCInfo(lcAccountSettings) << "Current folder encryption status code:" << status;
    auto job = qobject_cast<EncryptFolderJob*>(sender());
    Q_ASSERT(job);
    if (!job->errorString().isEmpty()) {
        QMessageBox::warning(nullptr, tr("Warning"), job->errorString());
    }

    const auto folder = job->property(propertyFolder).value<Folder *>();
    Q_ASSERT(folder);
    const auto path = job->property(propertyPath).toString();
    const auto index = _model->indexForPath(folder, path);
    Q_ASSERT(index.isValid());
    _model->resetAndFetch(index.parent());

    job->deleteLater();
}

QString AccountSettings::selectedFolderAlias() const
{
    const auto selected = _ui->_folderList->selectionModel()->currentIndex();
    if (!selected.isValid()) {
        return "";
    }
    return _model->data(selected, FolderStatusDelegate::FolderAliasRole).toString();
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
    // Make sure at least the root items are expanded
    for (int i = 0; i < _model->rowCount(); ++i) {
        const auto idx = _model->index(i);
        if (!_ui->_folderList->isExpanded(idx)) {
            _ui->_folderList->setExpanded(idx, true);
        }
    }
}

bool AccountSettings::canEncryptOrDecrypt(const FolderStatusModel::SubFolderInfo *info)
{
    if (const auto folderSyncStatus = info->_folder->syncResult().status(); info->_fileId.isEmpty()) {
        auto message = tr("Please wait for the folder to sync before trying to encrypt it.");
        if (folderSyncStatus == SyncResult::Status::Problem) {
            message = tr("The folder has a minor sync problem. Encryption of this folder will be possible once it has synced successfully");
        } else if (folderSyncStatus == SyncResult::Status::Error) {
            message = tr("The folder has a sync error. Encryption of this folder will be possible once it has synced successfully");
        }

        QMessageBox msgBox;
        msgBox.setText(message);
        msgBox.exec();
        return false;
    }

    if (!_accountState->account()->e2e() || !_accountState->account()->e2e()->isInitialized()) {
        QMessageBox msgBox;
        msgBox.setText(tr("You cannot encrypt this folder because the end-to-end encryption is not set-up yet on this device.\n"
                          "Would you like to do this now?"));
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Ok);
        const auto ret = msgBox.exec();

        switch (ret) {
        case QMessageBox::Ok:
            slotE2eEncryptionGenerateKeys();
            break;
        case QMessageBox::Cancel:
        default:
            break;
        }

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

void AccountSettings::slotMarkSubfolderEncrypted(FolderStatusModel::SubFolderInfo* folderInfo)
{
    if (!canEncryptOrDecrypt(folderInfo)) {
        return;
    }

    const auto folder = folderInfo->_folder;
    Q_ASSERT(folder);

    const auto folderAlias = folder->alias();
    const auto path = folderInfo->_path;
    const auto fileId = folderInfo->_fileId;
    const auto encryptFolder = [this, fileId, path, folderAlias] {
        const auto folder = FolderMan::instance()->folder(folderAlias);
        if (!folder) {
            qCWarning(lcAccountSettings) << "Could not encrypt folder because folder" << folderAlias << "does not exist anymore";
            QMessageBox::warning(nullptr, tr("Encryption failed"), tr("Could not encrypt folder because the folder does not exist anymore"));
            return;
        }

        // Folder info have directory paths in Foo/Bar/ convention...
        Q_ASSERT(!path.startsWith('/') && path.endsWith('/'));
        // But EncryptFolderJob expects directory path Foo/Bar convention
        const auto choppedPath = path.chopped(1);
        auto job = new OCC::EncryptFolderJob(accountsState()->account(), folder->journalDb(), choppedPath, choppedPath, folder->remotePath(), fileId);
        job->setParent(this);
        job->setProperty(propertyFolder, QVariant::fromValue(folder));
        job->setProperty(propertyPath, QVariant::fromValue(path));
        connect(job, &OCC::EncryptFolderJob::finished, this, &AccountSettings::slotEncryptFolderFinished);
        job->start();
    };

    if (folder->virtualFilesEnabled() && folder->vfs().mode() == Vfs::WindowsCfApi) {
        showEnableE2eeWithVirtualFilesWarningDialog(encryptFolder);
        return;
    }

    showEnableE2eeWarningDialog(encryptFolder);
}

void AccountSettings::slotEditCurrentIgnoredFiles()
{
    const auto folder = FolderMan::instance()->folder(selectedFolderAlias());
    if (!folder) {
        return;
    }
    openIgnoredFilesDialog(folder->path());
}

void AccountSettings::slotOpenMakeFolderDialog()
{
    const auto selected = _ui->_folderList->selectionModel()->currentIndex();

    if (!selected.isValid()) {
        qCWarning(lcAccountSettings) << "Selection model current folder index is not valid.";
        return;
    }

    const auto classification = _model->classify(selected);

    if (classification != FolderStatusModel::SubFolder && classification != FolderStatusModel::RootFolder) {
        return;
    }

    const auto folder = _model->infoForIndex(selected)->_folder;
    Q_ASSERT(folder);
    const auto fileName = [selected, classification, folder, this] {
        QString result;
        if (classification == FolderStatusModel::RootFolder) {
            result = folder->path();
        } else {
            result = _model->data(selected, FolderStatusDelegate::FolderPathRole).toString();
        }

        if (result.endsWith('/')) {
            result.chop(1);
        }

        return result;
    }();

    if (!fileName.isEmpty()) {
        const auto folderCreationDialog = new FolderCreationDialog(fileName, this);
        folderCreationDialog->setAttribute(Qt::WA_DeleteOnClose);
        folderCreationDialog->open();

#ifdef Q_OS_MACOS
        // The macOS FolderWatcher cannot detect file and folder changes made by the watching process -- us.
        // So we need to manually invoke the slot that is called by watched folder changes.
        connect(folderCreationDialog, &FolderCreationDialog::folderCreated, this, [folder, fileName](const QString &fullFolderPath) {
            folder->slotWatchedPathChanged(fullFolderPath, Folder::ChangeReason::Other);
        });
#endif
    }
}

void AccountSettings::slotEditCurrentLocalIgnoredFiles()
{
    const auto selected = _ui->_folderList->selectionModel()->currentIndex();
    if (!selected.isValid() || _model->classify(selected) != FolderStatusModel::SubFolder) {
        return;
    }
    const auto fileName = _model->data(selected, FolderStatusDelegate::FolderPathRole).toString();
    openIgnoredFilesDialog(fileName);
}

void AccountSettings::openIgnoredFilesDialog(const QString & absFolderPath)
{
    Q_ASSERT(QFileInfo(absFolderPath).isAbsolute());

    const QString ignoreFile{absFolderPath + ".sync-exclude.lst"};
    const auto layout = new QVBoxLayout();
    const auto ignoreListWidget = new IgnoreListTableWidget(this);
    ignoreListWidget->readIgnoreFile(ignoreFile);
    layout->addWidget(ignoreListWidget);

    const auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);

    const auto dialog = new QDialog();
    dialog->setLayout(layout);

    connect(buttonBox, &QDialogButtonBox::clicked, [=](QAbstractButton * button) {
        if (buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole) {
            ignoreListWidget->slotWriteIgnoreFile(ignoreFile);
        }
        dialog->close();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::close);

    dialog->open();
}

void AccountSettings::slotSubfolderContextMenuRequested(const QModelIndex& index, const QPoint& pos)
{
    Q_UNUSED(pos);

    QMenu menu;
    auto ac = menu.addAction(tr("Open folder"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotOpenCurrentLocalSubFolder);

    const auto fileName = _model->data(index, FolderStatusDelegate::FolderPathRole).toString();
    if (!QFile::exists(fileName)) {
        ac->setEnabled(false);
    }
    const auto info = _model->infoForIndex(index);
    const auto acc = _accountState->account();

    if (acc->capabilities().clientSideEncryptionAvailable()) {
        // Verify if the folder is empty before attempting to encrypt.

        const auto isEncrypted = info->isEncrypted();
        const auto isParentEncrypted = _model->isAnyAncestorEncrypted(index);
        const auto isTopFolder = index.parent().isValid() && !index.parent().parent().isValid();
        const auto isExternal = info->_isExternal;

        if (!isEncrypted && !isParentEncrypted && !isExternal && isTopFolder) {
            ac = menu.addAction(tr("Encrypt"));
            connect(ac, &QAction::triggered, [this, info] { slotMarkSubfolderEncrypted(info); });
        } else {
            // Ignore decrypting for now since it only works with an empty folder
            // connect(ac, &QAction::triggered, [this, &info] { slotMarkSubfolderDecrypted(info); });
        }
    }

    ac = menu.addAction(tr("Edit Ignored Files"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotEditCurrentLocalIgnoredFiles);

    ac = menu.addAction(tr("Create new folder"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotOpenMakeFolderDialog);
    ac->setEnabled(QFile::exists(fileName));

    const auto folder = info->_folder;
    if (folder && folder->virtualFilesEnabled()) {
        auto availabilityMenu = menu.addMenu(tr("Availability"));

        // Has '/' suffix convention for paths here but VFS and
        // sync engine expects no such suffix
        Q_ASSERT(info->_path.endsWith('/'));
        const auto remotePath = info->_path.chopped(1);

        // It might be an E2EE mangled path, so let's try to demangle it
        const auto journal = folder->journalDb();
        SyncJournalFileRecord rec;
        if (!journal->getFileRecordByE2eMangledName(remotePath, &rec)) {
            qCWarning(lcFolderStatus) << "Could not get file record by E2E Mangled Name from local DB" << remotePath;
        }

        const auto path = rec.isValid() ? rec._path : remotePath;

        ac = availabilityMenu->addAction(Utility::vfsPinActionText());
        connect(ac, &QAction::triggered, this, [this, folder, path] { slotSetSubFolderAvailability(folder, path, PinState::AlwaysLocal); });

        ac = availabilityMenu->addAction(Utility::vfsFreeSpaceActionText());
        connect(ac, &QAction::triggered, this, [this, folder, path] { slotSetSubFolderAvailability(folder, path, PinState::OnlineOnly); });
    }

    menu.exec(QCursor::pos());
}

void AccountSettings::slotCustomContextMenuRequested(const QPoint &pos)
{
    const auto treeView = _ui->_folderList;
    const auto index = treeView->indexAt(pos);
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

    treeView->setCurrentIndex(index);
    const auto alias = _model->data(index, FolderStatusDelegate::FolderAliasRole).toString();
    const auto folderPaused = _model->data(index, FolderStatusDelegate::FolderSyncPaused).toBool();
    const auto folderConnected = _model->data(index, FolderStatusDelegate::FolderAccountConnected).toBool();
    const auto folderMan = FolderMan::instance();
    const auto folder = folderMan->folder(alias);

    if (!folder) {
        return;
    }

    const auto menu = new QMenu(treeView);

    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto ac = menu->addAction(tr("Open folder"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotOpenCurrentFolder);

    ac = menu->addAction(tr("Edit Ignored Files"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotEditCurrentIgnoredFiles);

    ac = menu->addAction(tr("Create new folder"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotOpenMakeFolderDialog);
    ac->setEnabled(QFile::exists(folder->path()));

    if (!_ui->_folderList->isExpanded(index) && folder->supportsSelectiveSync()) {
        ac = menu->addAction(tr("Choose what to sync"));
        ac->setEnabled(folderConnected);
        connect(ac, &QAction::triggered, this, &AccountSettings::doExpand);
    }

    if (!folderPaused) {
        ac = menu->addAction(tr("Force sync now"));
        if (folder && folder->isSyncRunning()) {
            ac->setText(tr("Restart sync"));
        }
        ac->setEnabled(folderConnected);
        connect(ac, &QAction::triggered, this, &AccountSettings::slotForceSyncCurrentFolder);
    }

    ac = menu->addAction(folderPaused ? tr("Resume sync") : tr("Pause sync"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotEnableCurrentFolder);

    ac = menu->addAction(tr("Remove folder sync connection"));
    connect(ac, &QAction::triggered, this, &AccountSettings::slotRemoveCurrentFolder);

    if (folder->virtualFilesEnabled()) {
        auto availabilityMenu = menu->addMenu(tr("Availability"));

        ac = availabilityMenu->addAction(Utility::vfsPinActionText());
        connect(ac, &QAction::triggered, this, [this]() { slotSetCurrentFolderAvailability(PinState::AlwaysLocal); });
        ac->setDisabled(Theme::instance()->enforceVirtualFilesSyncFolder());

        ac = availabilityMenu->addAction(Utility::vfsFreeSpaceActionText());
        connect(ac, &QAction::triggered, this, [this]() { slotSetCurrentFolderAvailability(PinState::OnlineOnly); });

        ac = menu->addAction(tr("Disable virtual file support …"));
        connect(ac, &QAction::triggered, this, &AccountSettings::slotDisableVfsCurrentFolder);
        ac->setDisabled(Theme::instance()->enforceVirtualFilesSyncFolder());
    }

    if (const auto mode = bestAvailableVfsMode();
        !Theme::instance()->disableVirtualFilesSyncFolder() &&
        Theme::instance()->showVirtualFilesOption() && !folder->virtualFilesEnabled() && Vfs::checkAvailability(folder->path(), mode)) {
        if (mode == Vfs::WindowsCfApi || ConfigFile().showExperimentalOptions()) {
            ac = menu->addAction(tr("Enable virtual file support %1 …").arg(mode == Vfs::WindowsCfApi ? QString() : tr("(experimental)")));
            // TODO: remove when UX decision is made
            ac->setEnabled(!Utility::isPathWindowsDrivePartitionRoot(folder->path()));
            //
            connect(ac, &QAction::triggered, this, &AccountSettings::slotEnableVfsCurrentFolder);
        }
    }


    menu->popup(treeView->mapToGlobal(pos));
}

void AccountSettings::slotFolderListClicked(const QModelIndex &indx)
{
    if (indx.data(FolderStatusDelegate::AddButton).toBool()) {
        // "Add Folder Sync Connection"
        const auto treeView = _ui->_folderList;
        const auto pos = treeView->mapFromGlobal(QCursor::pos());
        QStyleOptionViewItem opt;
        opt.initFrom(treeView);
        const auto btnRect = treeView->visualRect(indx);
        const auto btnSize = treeView->itemDelegateForIndex(indx)->sizeHint(opt, indx);
        const auto actual = QStyle::visualRect(opt.direction, btnRect, QRect(btnRect.topLeft(), btnSize));
        if (!actual.contains(pos)) {
            return;
        }

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
        const auto treeView = _ui->_folderList;
        const auto pos = treeView->mapFromGlobal(QCursor::pos());
        if (FolderStatusDelegate::optionsButtonRect(treeView->visualRect(indx), layoutDirection()).contains(pos)) {
            slotCustomContextMenuRequested(pos);
            return;
        }
        if (FolderStatusDelegate::errorsListRect(treeView->visualRect(indx)).contains(pos)) {
            emit showIssuesList(_accountState);
            return;
        }

        // Expand root items on single click
        if (_accountState && _accountState->state() == AccountState::Connected) {
            const auto expanded = !(_ui->_folderList->isExpanded(indx));
            _ui->_folderList->setExpanded(indx, expanded);
        }
    }
}

void AccountSettings::slotAddFolder()
{
    const auto folderMan = FolderMan::instance();
    folderMan->setSyncEnabled(false); // do not start more syncs.

    const auto folderWizard = new FolderWizard(_accountState->account(), this);
    folderWizard->setAttribute(Qt::WA_DeleteOnClose);

    connect(folderWizard, &QDialog::accepted, this, &AccountSettings::slotFolderWizardAccepted);
    connect(folderWizard, &QDialog::rejected, this, &AccountSettings::slotFolderWizardRejected);
    folderWizard->open();
}


void AccountSettings::slotFolderWizardAccepted()
{
    const auto folderWizard = qobject_cast<FolderWizard *>(sender());
    const auto folderMan = FolderMan::instance();

    qCInfo(lcAccountSettings) << "Folder wizard completed";

    FolderDefinition definition;
    definition.localPath = FolderDefinition::prepareLocalPath(
        folderWizard->field(QLatin1String("sourceFolder")).toString());
    definition.targetPath = FolderDefinition::prepareTargetPath(
        folderWizard->property("targetPath").toString());

    if (folderWizard->property("useVirtualFiles").toBool()) {
        definition.virtualFilesMode = bestAvailableVfsMode();
    }

    {
        QDir dir(definition.localPath);
        if (!dir.exists()) {
            qCInfo(lcAccountSettings) << "Creating folder" << definition.localPath;
            if (!dir.mkpath(".")) {
                QMessageBox::warning(this, tr("Folder creation failed"),
                    tr("<p>Could not create local folder <i>%1</i>.</p>")
                        .arg(Utility::escape(QDir::toNativeSeparators(definition.localPath))));
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

#ifdef Q_OS_WIN
    if (folderMan->navigationPaneHelper().showInExplorerNavigationPane()) {
        definition.navigationPaneClsid = QUuid::createUuid();
    }
#endif

    const auto selectiveSyncBlackList = folderWizard->property("selectiveSyncBlackList").toStringList();

    folderMan->setSyncEnabled(true);

    const auto folder = folderMan->addFolder(_accountState, definition);
    if (folder) {
        if (definition.virtualFilesMode != Vfs::Off && folderWizard->property("useVirtualFiles").toBool()) {
            folder->setRootPinState(PinState::OnlineOnly);
        }

        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, selectiveSyncBlackList);

        // The user already accepted the selective sync dialog. everything is in the white list
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList,
            QStringList() << QLatin1String("/"));
        folderMan->scheduleAllFolders();
        emit folderChanged();
    }
}

void AccountSettings::slotFolderWizardRejected()
{
    qCInfo(lcAccountSettings) << "Folder wizard cancelled";
    const auto folderMan = FolderMan::instance();
    folderMan->setSyncEnabled(true);
}

void AccountSettings::slotRemoveCurrentFolder()
{
    const auto folder = FolderMan::instance()->folder(selectedFolderAlias());
    const auto selected = _ui->_folderList->selectionModel()->currentIndex();
    if (selected.isValid() && folder) {
        const auto row = selected.row();

        qCInfo(lcAccountSettings) << "Remove Folder alias " << folder->alias();
        const auto shortGuiLocalPath = folder->shortGuiLocalPath();

        auto messageBox = new QMessageBox(QMessageBox::Question,
            tr("Confirm Folder Sync Connection Removal"),
            tr("<p>Do you really want to stop syncing the folder <i>%1</i>?</p>"
               "<p><b>Note:</b> This will <b>not</b> delete any files.</p>")
                .arg(shortGuiLocalPath),
            QMessageBox::NoButton,
            this);
        messageBox->setAttribute(Qt::WA_DeleteOnClose);
        const auto yesButton = messageBox->addButton(tr("Remove Folder Sync Connection"), QMessageBox::YesRole);
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
    }
}

void AccountSettings::slotOpenCurrentFolder()
{
    const auto alias = selectedFolderAlias();
    if (!alias.isEmpty()) {
        emit openFolderAlias(alias);
    }
}

void AccountSettings::slotOpenCurrentLocalSubFolder()
{
    const auto selected = _ui->_folderList->selectionModel()->currentIndex();
    if (!selected.isValid() || _model->classify(selected) != FolderStatusModel::SubFolder) {
        return;
    }
    const auto fileName = _model->data(selected, FolderStatusDelegate::FolderPathRole).toString();
    const auto url = QUrl::fromLocalFile(fileName);
    QDesktopServices::openUrl(url);
}

void AccountSettings::slotEnableVfsCurrentFolder()
{
    const auto folderMan = FolderMan::instance();
    const auto folder = folderMan->folder(selectedFolderAlias());
    const auto selected = _ui->_folderList->selectionModel()->currentIndex();

    if (!selected.isValid() || !folder) {
        return;
    }

    OwncloudWizard::askExperimentalVirtualFilesFeature(this, [folder, this](bool enable) {
        if (!enable || !folder) {
            return;
        }

#ifdef Q_OS_WIN
        // we might need to add or remove the panel entry as cfapi brings this feature out of the box
        FolderMan::instance()->navigationPaneHelper().scheduleUpdateCloudStorageRegistry();
#endif

        // It is unsafe to switch on vfs while a sync is running - wait if necessary.
        const auto connection = std::make_shared<QMetaObject::Connection>();
        const auto switchVfsOn = [folder, connection, this]() {
            if (*connection) {
                QObject::disconnect(*connection);
            }

            qCInfo(lcAccountSettings) << "Enabling vfs support for folder" << folder->path();

            // Wipe selective sync blacklist
            auto ok = false;
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
                if (FileSystem::fileExists(entry) && !folder->vfs().setPinState(entry, PinState::OnlineOnly)) {
                    qCWarning(lcAccountSettings) << "Could not set pin state of" << entry << "to online only";
                }
            }
            folder->slotNextSyncFullLocalDiscovery();

            FolderMan::instance()->scheduleFolder(folder);

            _ui->_folderList->doItemsLayout();
            _ui->selectiveSyncStatus->setVisible(false);
        };

        if (folder->isSyncRunning()) {
            *connection = connect(folder, &Folder::syncFinished, this, switchVfsOn);
            folder->setVfsOnOffSwitchPending(true);
            folder->slotTerminateSync();
            _ui->_folderList->doItemsLayout();
        } else {
            switchVfsOn();
        }
    });
}

void AccountSettings::slotDisableVfsCurrentFolder()
{
    const auto folderMan = FolderMan::instance();
    const auto folder = folderMan->folder(selectedFolderAlias());
    const auto selected = _ui->_folderList->selectionModel()->currentIndex();

    if (!selected.isValid() || !folder) {
        return;
    }

    const auto msgBox = new QMessageBox(
        QMessageBox::Question,
        tr("Disable virtual file support?"),
        tr("This action will disable virtual file support. As a consequence contents of folders that "
           "are currently marked as \"available online only\" will be downloaded."
           "\n\n"
           "The only advantage of disabling virtual file support is that the selective sync feature "
           "will become available again."
           "\n\n"
           "This action will abort any currently running synchronization."));
    const auto acceptButton = msgBox->addButton(tr("Disable support"), QMessageBox::AcceptRole);
    msgBox->addButton(tr("Cancel"), QMessageBox::RejectRole);
    connect(msgBox, &QMessageBox::finished, msgBox, [this, msgBox, folder, acceptButton] {
        msgBox->deleteLater();
        if (msgBox->clickedButton() != acceptButton|| !folder) {
            return;
        }

#ifdef Q_OS_WIN
        // we might need to add or remove the panel entry as cfapi brings this feature out of the box
        FolderMan::instance()->navigationPaneHelper().scheduleUpdateCloudStorageRegistry();
#endif

        // It is unsafe to switch off vfs while a sync is running - wait if necessary.
        const auto connection = std::make_shared<QMetaObject::Connection>();
        const auto switchVfsOff = [folder, connection, this]() {
            if (*connection) {
                QObject::disconnect(*connection);
            }

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

            _ui->_folderList->doItemsLayout();
        };

        if (folder->isSyncRunning()) {
            *connection = connect(folder, &Folder::syncFinished, this, switchVfsOff);
            folder->setVfsOnOffSwitchPending(true);
            folder->slotTerminateSync();
            _ui->_folderList->doItemsLayout();
        } else {
            switchVfsOff();
        }
    });
    msgBox->open();
}

void AccountSettings::slotSetCurrentFolderAvailability(PinState state)
{
    ASSERT(state == PinState::OnlineOnly || state == PinState::AlwaysLocal);

    const auto folderMan = FolderMan::instance();
    const auto folder = folderMan->folder(selectedFolderAlias());
    const auto selected = _ui->_folderList->selectionModel()->currentIndex();

    if (!selected.isValid() || !folder) {
        return;
    }

    // similar to socket api: sets pin state recursively and sync
    folder->setRootPinState(state);
    folder->scheduleThisFolderSoon();
}

void AccountSettings::slotSetSubFolderAvailability(Folder *folder, const QString &path, PinState state)
{
    Q_ASSERT(folder && folder->virtualFilesEnabled());
    Q_ASSERT(!path.endsWith('/'));

    // Update the pin state on all items
    if (!folder->vfs().setPinState(path, state)) {
        qCWarning(lcAccountSettings) << "Could not set pin state of" << path << "to" << state;
    }

    // Trigger sync
    folder->schedulePathForLocalDiscovery(path);
    folder->scheduleThisFolderSoon();
}

void AccountSettings::displayMnemonic(const QString &mnemonic)
{
    QDialog widget;
    Ui_Dialog ui{};
    ui.setupUi(&widget);
    widget.setWindowTitle(tr("End-to-end encryption mnemonic"));
    ui.label->setText(
        tr("To protect your Cryptographic Identity, we encrypt it with a mnemonic of 12 dictionary words. "
           "Please note it down and keep it safe. "
           "You will need it to set-up the synchronization of encrypted folders on your other devices."));
    QFont monoFont(QStringLiteral("Monospace"));
    monoFont.setStyleHint(QFont::TypeWriter);
    ui.lineEdit->setFont(monoFont);
    ui.lineEdit->setText(mnemonic);
    ui.lineEdit->setReadOnly(true);

    ui.lineEdit->setStyleSheet(QStringLiteral("QLineEdit{ color: black; background: lightgrey; border-style: inset;}"));

    ui.lineEdit->focusWidget();
    ui.lineEdit->selectAll();
    ui.lineEdit->setAlignment(Qt::AlignCenter);

    const QFont font(QStringLiteral(""), 0);
    QFontMetrics fm(font);
    ui.lineEdit->setFixedWidth(fm.horizontalAdvance(mnemonic));
    widget.resize(widget.sizeHint());
    widget.exec();
}

void AccountSettings::forgetEncryptionOnDeviceForAccount(const AccountPtr &account) const
{
    QMessageBox dialog;
    dialog.setWindowTitle(tr("Forget the end-to-end encryption on this device"));
    dialog.setText(tr("Do you want to forget the end-to-end encryption settings for %1 on this device?").arg(account->davUser()));
    dialog.setInformativeText(tr("Forgetting end-to-end encryption will remove the sensitive data and all the encrypted files from this device."
                                 "<br>"
                                 "However, the encrypted files will remain on the server and all your other devices, if configured."));
    dialog.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    dialog.setDefaultButton(QMessageBox::Ok);
    dialog.adjustSize();

    const auto ret = dialog.exec();
    switch(ret) {
    case QMessageBox::Ok:
        connect(account->e2e(), &ClientSideEncryption::sensitiveDataForgotten,
                this, &AccountSettings::forgetE2eEncryption);
        account->e2e()->forgetSensitiveData();
        break;
    case QMessageBox::Cancel:
        break;
    Q_UNREACHABLE();
    }
}

void AccountSettings::migrateCertificateForAccount(const AccountPtr &account)
{
    const auto &allActions = _ui->encryptionMessage->actions();
    for (const auto action : allActions) {
        _ui->encryptionMessage->removeAction(action);
    }
    updateEncryptionMessageActions();

    account->e2e()->migrateCertificate();
    slotE2eEncryptionGenerateKeys();
}

void AccountSettings::showConnectionLabel(const QString &message, QStringList errors)
{
    const auto errStyle = QLatin1String("color:#ffffff; background-color:#bb4d4d;padding:5px;"
                                        "border-width: 1px; border-style: solid; border-color: #aaaaaa;"
                                        "border-radius:5px;");
    if (errors.isEmpty()) {
        auto msg = message;
        Theme::replaceLinkColorStringBackgroundAware(msg);
        _ui->connectLabel->setText(msg);
        _ui->connectLabel->setToolTip({});
        _ui->connectLabel->setStyleSheet({});
    } else {
        errors.prepend(message);
        auto userFriendlyMsg = errors.join(QLatin1String("<br>"));
        qCDebug(lcAccountSettings) << userFriendlyMsg;
        Theme::replaceLinkColorString(userFriendlyMsg, QColor(0xc1c8e6));
        _ui->connectLabel->setText(userFriendlyMsg);
        _ui->connectLabel->setToolTip({});
        _ui->connectLabel->setStyleSheet(errStyle);
    }
    _ui->accountStatus->setVisible(!message.isEmpty());
}

void AccountSettings::slotEnableCurrentFolder(bool terminate)
{
    const auto alias = selectedFolderAlias();

    if (!alias.isEmpty()) {
        const auto folderMan = FolderMan::instance();

        qCInfo(lcAccountSettings) << "Application: enable folder with alias " << alias;
        auto currentlyPaused = false;

        // this sets the folder status to disabled but does not interrupt it.
        const auto folder = folderMan->folder(alias);
        if (!folder) {
            return;
        }
        currentlyPaused = folder->syncPaused();
        if (!currentlyPaused && !terminate) {
            // check if a sync is still running and if so, ask if we should terminate.
            if (folder->isBusy()) { // its still running
                const auto msgbox = new QMessageBox(QMessageBox::Question, tr("Sync Running"),
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
        if (folder->isBusy() && terminate) {
            folder->slotTerminateSync();
        }
        folder->setSyncPaused(!currentlyPaused);

        // keep state for the icon setting.
        if (currentlyPaused) {
            _wasDisabledBefore = true;
        }

        _model->slotUpdateFolderState(folder);
    }
}

void AccountSettings::slotScheduleCurrentFolder()
{
    const auto folderMan = FolderMan::instance();
    if (auto folder = folderMan->folder(selectedFolderAlias())) {
        folderMan->scheduleFolder(folder);
    }
}

void AccountSettings::slotScheduleCurrentFolderForceRemoteDiscovery()
{
    const auto folderMan = FolderMan::instance();
    if (auto folder = folderMan->folder(selectedFolderAlias())) {
        folder->slotWipeErrorBlacklist();
        folder->journalDb()->forceRemoteDiscoveryNextSync();
        folderMan->scheduleFolder(folder);
    }
}

void AccountSettings::slotForceSyncCurrentFolder()
{
    FolderMan *folderMan = FolderMan::instance();
    auto selectedFolder = folderMan->folder(selectedFolderAlias());
    folderMan->forceSyncForFolder(selectedFolder);
}

void AccountSettings::slotOpenOC()
{
    if (_OCUrl.isValid()) {
        Utility::openBrowser(_OCUrl);
    }
}

void AccountSettings::slotUpdateQuota(qint64 total, qint64 used)
{
    if (total > 0) {
        const auto usedStr = Utility::octetsToString(used);
        const auto totalStr = Utility::octetsToString(total);
        _spaceUsageText = tr("%1 of %2 in use").arg(usedStr, totalStr);
    } else {
        /* -1 means not computed; -2 means unknown; -3 means unlimited  (#owncloud/client/issues/3940)*/
        if (total == 0 || total == -1) {
            _spaceUsageText.clear();
        } else {
            const auto usedStr = Utility::octetsToString(used);
            _spaceUsageText = tr("%1 in use").arg(usedStr);
        }
    }

    slotAccountStateChanged();
}

void AccountSettings::slotAccountStateChanged()
{
    const auto state = _accountState ? _accountState->state() : AccountState::Disconnected;
    if (state != AccountState::Disconnected) {
        _ui->sslButton->updateAccountState(_accountState);
        const auto account = _accountState->account();
        auto safeUrl = account->url();
        safeUrl.setPassword({}); // Remove the password from the URL to avoid showing it in the UI
        const auto folders = FolderMan::instance()->map().values();
        for (const auto folder : folders) {
            _model->slotUpdateFolderState(folder);
        }

        const auto server = QString::fromLatin1("<a href=\"%1\">%2</a>")
                                .arg(Utility::escape(account->url().toString()),
                                    Utility::escape(safeUrl.toString()));
        auto serverWithUser = server;
        if (const auto cred = account->credentials()) {
            auto user = account->davDisplayName();
            if (user.isEmpty()) {
                user = cred->user();
            }
            serverWithUser = tr("%1 as %2").arg(server, Utility::escape(user));
        }

        switch (state) {
        case AccountState::Connected: {
            QStringList errors;
            if (account->serverVersionUnsupported()) {
                errors << tr("The server version %1 is unsupported! Proceed at your own risk.").arg(account->serverVersion());
            }
            auto statusMessage = tr("Connected to %1.").arg(serverWithUser);
            if (!_spaceUsageText.isEmpty()) {
                statusMessage = tr("Connected to %1 (%2).").arg(serverWithUser, _spaceUsageText);
            }
            showConnectionLabel(statusMessage, errors);
            break;
        }
        case AccountState::ServiceUnavailable:
            showConnectionLabel(tr("Server %1 is temporarily unavailable.").arg(server));
            break;
        case AccountState::MaintenanceMode:
            showConnectionLabel(tr("Server %1 is currently in maintenance mode.").arg(server));
            break;
        case AccountState::RedirectDetected:
            showConnectionLabel(tr("Server %1 is currently being redirected, or your connection is behind a captive portal.").arg(server));
            break;
        case AccountState::SignedOut:
            showConnectionLabel(tr("Signed out from %1.").arg(serverWithUser));
            break;
        case AccountState::AskingCredentials: {
            showConnectionLabel(tr("Connecting to %1 …").arg(serverWithUser));
            break;
        }
        case AccountState::NetworkError:
            showConnectionLabel(tr("Unable to connect to %1.")
                                    .arg(Utility::escape(Theme::instance()->appNameGUI())),
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
        case AccountState::NeedToSignTermsOfService:
            showConnectionLabel(tr("You need to accept the terms of service at %1.").arg(server));
            break;
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
        for (auto i = 0; i < _model->rowCount(); ++i) {
            if (_ui->_folderList->isExpanded(_model->index(i))) {
                _ui->_folderList->setExpanded(_model->index(i), false);
            }
        }
    } else if (_model->isDirty()) {
        // If we connect and have pending changes, show the list.
        doExpand();
    }

    // Disabling expansion of folders might require hiding the selective
    // sync user interface buttons.
    refreshSelectiveSyncStatus();

    if (state == AccountState::State::Connected) {
        checkClientSideEncryptionState();
    }
}

void AccountSettings::checkClientSideEncryptionState()
{
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

void AccountSettings::slotLinkActivated(const QString &link)
{
    // Parse folder alias and filename from the link, calculate the index
    // and select it if it exists.
    const auto li = link.split(QLatin1String("?folder="));
    if (li.count() > 1) {
        auto myFolder = li[0];
        const auto alias = li[1];
        if (myFolder.endsWith(QLatin1Char('/'))) {
            myFolder.chop(1);
        }

        // Make sure the folder itself is expanded
        const auto folder = FolderMan::instance()->folder(alias);
        const auto folderIndx = _model->indexForPath(folder, {});
        if (!_ui->_folderList->isExpanded(folderIndx)) {
            _ui->_folderList->setExpanded(folderIndx, true);
        }

        const auto indx = _model->indexForPath(folder, myFolder);
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

void AccountSettings::slotHideSelectiveSyncWidget()
{
    _ui->selectiveSyncApply->setEnabled(false);
    _ui->selectiveSyncStatus->setVisible(false);
    _ui->selectiveSyncButtons->setVisible(false);
    _ui->selectiveSyncLabel->hide();
}

void AccountSettings::slotSelectiveSyncChanged(const QModelIndex &topLeft,
                                               const QModelIndex &bottomRight,
                                               const QVector<int> &roles)
{
    Q_UNUSED(bottomRight);
    if (!roles.contains(Qt::CheckStateRole)) {
        return;
    }

    const auto info = _model->infoForIndex(topLeft);
    if (!info) {
        return;
    }

    const auto showWarning = _model->isDirty() && _accountState->isConnected() && info->_checked == Qt::Unchecked;

    // FIXME: the model is not precise enough to handle extra cases
    // e.g. the user clicked on the same checkbox 2x without applying the change in between.
    // We don't know which checkbox changed to be able to toggle the selectiveSyncLabel display.
    if (showWarning) {
        _ui->selectiveSyncLabel->show();
    }

    const auto shouldBeVisible = _model->isDirty();
    const auto wasVisible = _ui->selectiveSyncStatus->isVisible();
    if (shouldBeVisible) {
        _ui->selectiveSyncStatus->setVisible(true);
    }

    _ui->selectiveSyncApply->setEnabled(true);
    _ui->selectiveSyncButtons->setVisible(true);

    if (shouldBeVisible != wasVisible) {
        const auto hint = _ui->selectiveSyncStatus->sizeHint();

        if (shouldBeVisible) {
            _ui->selectiveSyncStatus->setMaximumHeight(0);
        }

        const auto anim = new QPropertyAnimation(_ui->selectiveSyncStatus, "maximumHeight", _ui->selectiveSyncStatus);
        anim->setEndValue(_model->isDirty() ? hint.height() : 0);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        connect(anim, &QPropertyAnimation::finished, [this, shouldBeVisible]() {
            _ui->selectiveSyncStatus->setMaximumHeight(QWIDGETSIZE_MAX);
            if (!shouldBeVisible) {
                _ui->selectiveSyncStatus->hide();
            }
        });
    }
}

void AccountSettings::slotPossiblyUnblacklistE2EeFoldersAndRestartSync()
{
    if (!_accountState->account()->e2e()->isInitialized()) {
        return;
    }

    disconnect(_accountState->account()->e2e(), &ClientSideEncryption::initializationFinished, this, &AccountSettings::slotPossiblyUnblacklistE2EeFoldersAndRestartSync);

    for (const auto folder : FolderMan::instance()->map()) {
        if (folder->accountState() != _accountState) {
            continue;
        }
        bool ok = false;
        const auto foldersToRemoveFromBlacklist = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist, &ok);
        if (foldersToRemoveFromBlacklist.isEmpty()) {
            continue;
        }
        auto blackList = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok);
        const auto blackListSize = blackList.size();
        if (blackListSize == 0) {
            continue;
        }
        for (const auto &pathToRemoveFromBlackList : foldersToRemoveFromBlacklist) {
            blackList.removeAll(pathToRemoveFromBlackList);
        }
        if (blackList.size() != blackListSize) {
            if (folder->isSyncRunning()) {
                folderTerminateSyncAndUpdateBlackList(blackList, folder, foldersToRemoveFromBlacklist);
                return;
            }
            updateBlackListAndScheduleFolderSync(blackList, folder, foldersToRemoveFromBlacklist);
        }
    }
}

void AccountSettings::slotE2eEncryptionCertificateNeedMigration()
{
    const auto actionMigrateCertificate = addActionToEncryptionMessage(tr("Migrate certificate to a new one"), e2EeUiActionMigrateCertificateId);
    connect(actionMigrateCertificate, &QAction::triggered, this, [this] {
        migrateCertificateForAccount(_accountState->account());
    });
}

void AccountSettings::updateBlackListAndScheduleFolderSync(const QStringList &blackList, OCC::Folder *folder, const QStringList &foldersToRemoveFromBlacklist) const
{
    folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, blackList);
    folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist, {});
    for (const auto &pathToRemoteDiscover : foldersToRemoveFromBlacklist) {
        folder->journalDb()->schedulePathForRemoteDiscovery(pathToRemoteDiscover);
    }
    FolderMan::instance()->scheduleFolder(folder);
}

void AccountSettings::folderTerminateSyncAndUpdateBlackList(const QStringList &blackList, OCC::Folder *folder, const QStringList &foldersToRemoveFromBlacklist)
{
    if (_folderConnections.contains(folder->alias())) {
        qCWarning(lcAccountSettings) << "Folder " << folder->alias() << "is already terminating the sync.";
        return;
    }
    // in case sync is already running - terminate it and start a new one
    const QMetaObject::Connection syncTerminatedConnection = connect(folder, &Folder::syncFinished, this, [this, blackList, folder, foldersToRemoveFromBlacklist]() {
        const auto foundConnectionIt = _folderConnections.find(folder->alias());
        if (foundConnectionIt != _folderConnections.end()) {
            disconnect(*foundConnectionIt);
            _folderConnections.erase(foundConnectionIt);
        }
        updateBlackListAndScheduleFolderSync(blackList, folder, foldersToRemoveFromBlacklist);
    });
    _folderConnections.insert(folder->alias(), syncTerminatedConnection);
    folder->slotTerminateSync();
}

void AccountSettings::refreshSelectiveSyncStatus()
{
    QString unsyncedFoldersString;
    QString becameBigFoldersString;

    const auto folders = FolderMan::instance()->map().values();

    static const auto folderSeparatorString = QStringLiteral(", ");
    static const auto folderLinkString = [](const QString &slashlessFolderPath, const QString &folderName) {
        return QStringLiteral("<a href=\"%1?folder=%2\">%1</a>").arg(slashlessFolderPath, folderName);
    };
    static const auto appendFolderDisplayString = [](QString &foldersString, const QString &folderDisplayString) {
        if (!foldersString.isEmpty()) {
            foldersString += folderSeparatorString;
        }
        foldersString += folderDisplayString;
    };

    _ui->bigFolderUi->setVisible(false);

    for (const auto folder : folders) {
        if (folder->accountState() != _accountState) {
            continue;
        }

        auto ok = false;
        auto blacklistOk = false;
        const auto undecidedList = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, &ok);
        auto blacklist = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &blacklistOk);
        blacklist.sort();

        for (const auto &it : undecidedList) {
            // FIXME: add the folder alias in a hoover hint.
            // folder->alias() + QLatin1String("/")

            const auto folderTrailingSlash = Utility::trailingSlashPath(it);
            const auto folderWithoutTrailingSlash = it.endsWith('/') ? it.left(it.length() - 1) : it;
            const auto escapedFolderString = Utility::escape(folderWithoutTrailingSlash);
            const auto escapedFolderName = Utility::escape(folder->alias());
            const auto folderIdx = _model->indexForPath(folder, folderWithoutTrailingSlash);

            // If we do not know the index yet then do not provide a link string
            const auto folderDisplayString = folderIdx.isValid() ? folderLinkString(escapedFolderString, escapedFolderName) : folderWithoutTrailingSlash;

            // The new big folder procedure automatically places these new big folders in the blacklist.
            // This is not the case for existing folders discovered to have gone beyond the limit.
            // So we need to check if the folder is in the blacklist or not and tweak the message accordingly.
            if (SyncJournalDb::findPathInSelectiveSyncList(blacklist, folderTrailingSlash)) {
                appendFolderDisplayString(unsyncedFoldersString, folderDisplayString);
            } else {
                appendFolderDisplayString(becameBigFoldersString, folderDisplayString);
            }
        }
    }

    ConfigFile cfg;
    QString infoString;

    if (!unsyncedFoldersString.isEmpty()) {
        infoString += !cfg.confirmExternalStorage() ? tr("There are folders that were not synchronized because they are too big: ")
            : !cfg.newBigFolderSizeLimit().first    ? tr("There are folders that were not synchronized because they are external storages: ")
                                                    : tr("There are folders that were not synchronized because they are too big or external storages: ");

        infoString += unsyncedFoldersString;
    }

    if (!becameBigFoldersString.isEmpty()) {
        if (!infoString.isEmpty()) {
            infoString += QStringLiteral("\n");
        }

        const auto folderSizeLimitString = QString::number(cfg.newBigFolderSizeLimit().second);
        infoString += tr("There are folders that have grown in size beyond %1MB: %2").arg(folderSizeLimitString, becameBigFoldersString);
    }

    _ui->selectiveSyncNotification->setText(infoString);
    _ui->bigFolderUi->setVisible(!infoString.isEmpty());
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
    auto msg = _ui->connectLabel->text();
    Theme::replaceLinkColorStringBackgroundAware(msg);
    _ui->connectLabel->setText(msg);
}

void AccountSettings::setupE2eEncryption()
{
    connect(_accountState->account()->e2e(), &ClientSideEncryption::initializationFinished, this, &AccountSettings::slotPossiblyUnblacklistE2EeFoldersAndRestartSync);

    if (_accountState->account()->e2e()->isInitialized()) {
        slotE2eEncryptionMnemonicReady();
    } else {
        setupE2eEncryptionMessage();

        connect(_accountState->account()->e2e(), &ClientSideEncryption::initializationFinished, this, [this] {
            if (!_accountState->account()->e2e()->getPublicKey().isNull()) {
                _ui->encryptionMessageLabel->setText(tr("End-to-end encryption has been initialized on this account with another device."
                                                        "<br>"
                                                        "Enter the unique mnemonic to have the encrypted folders synchronize on this device as well."));
            }
        });
        _accountState->account()->setE2eEncryptionKeysGenerationAllowed(false);
        _accountState->account()->e2e()->initialize(this);
    }
}

void AccountSettings::forgetE2eEncryption()
{
    const auto &allActions = _ui->encryptionMessage->actions();
    for (const auto action : allActions) {
        _ui->encryptionMessage->removeAction(action);
    }
    updateEncryptionMessageActions();
    _ui->encryptionMessageLabel->setText({});
    setEncryptionMessageIcon({});
    setupE2eEncryptionMessage();
    checkClientSideEncryptionState();

    const auto account = _accountState->account();
    if (!account->e2e()->isInitialized()) {
        FolderMan::instance()->removeE2eFiles(account);
    }
}

void AccountSettings::removeActionFromEncryptionMessage(const QString &actionId)
{
    const auto foundEnableEncryptionActionIt = std::find_if(std::cbegin(_ui->encryptionMessage->actions()), std::cend(_ui->encryptionMessage->actions()), [&actionId](const QAction *action) {
        return action->property(e2eUiActionIdKey).toString() == actionId;
    });
    if (foundEnableEncryptionActionIt != std::cend(_ui->encryptionMessage->actions())) {
        _ui->encryptionMessage->removeAction(*foundEnableEncryptionActionIt);
        (*foundEnableEncryptionActionIt)->deleteLater();
        updateEncryptionMessageActions();
    }
}

QAction *AccountSettings::addActionToEncryptionMessage(const QString &actionTitle, const QString &actionId)
{
    const auto encryptionActions = _ui->encryptionMessage->actions();
    for (const auto &action : encryptionActions) {
        if (action->property(e2eUiActionIdKey) == actionId) {
            return action;
        }
    }

    auto *const action = new QAction(actionTitle, this);
    if (!actionId.isEmpty()) {
        action->setProperty(e2eUiActionIdKey, actionId);
    }
    _ui->encryptionMessage->addAction(action);
    updateEncryptionMessageActions();
    return action;
}

void AccountSettings::setupE2eEncryptionMessage()
{
    _ui->encryptionMessageLabel->setText(tr("This account supports end-to-end encryption, but it needs to be set up first."));
    setEncryptionMessageIcon(Theme::createColorAwareIcon(QStringLiteral(":/client/theme/info.svg")));
    _ui->encryptionMessage->hide();

    auto *const actionSetupE2e = addActionToEncryptionMessage(tr("Set up encryption"), e2EeUiActionSetupEncryptionId);
    connect(actionSetupE2e, &QAction::triggered, this, &AccountSettings::slotE2eEncryptionGenerateKeys);
}

void AccountSettings::setEncryptionMessageIcon(const QIcon &icon)
{
    if (icon.isNull()) {
        _ui->encryptionMessageIcon->clear();
        _ui->encryptionMessageIcon->hide();
        return;
    }

    const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    _ui->encryptionMessageIcon->setPixmap(icon.pixmap(iconSize, iconSize));
    _ui->encryptionMessageIcon->show();
}

void AccountSettings::updateEncryptionMessageActions()
{
    for (auto buttonIt = _encryptionMessageButtons.begin(); buttonIt != _encryptionMessageButtons.end(); ++buttonIt) {
        _ui->encryptionMessageButtonsLayout->removeWidget(buttonIt.value());
        buttonIt.value()->deleteLater();
    }
    _encryptionMessageButtons.clear();

    const auto actions = _ui->encryptionMessage->actions();
    auto *layout = _ui->encryptionMessageButtonsLayout;
    int stretchIndex = -1;
    for (int i = 0; i < layout->count(); ++i) {
        if (layout->itemAt(i)->spacerItem()) {
            stretchIndex = i;
            break;
        }
    }
    if (stretchIndex == -1) {
        layout->addStretch();
        stretchIndex = layout->count() - 1;
    }

    for (QAction *action : actions) {
        auto *button = new QPushButton(_ui->encryptionMessage);
        button->setText(action->text());
        button->setIcon(action->icon());
        button->setEnabled(action->isEnabled());
        button->setVisible(action->isVisible());
        connect(button, &QPushButton::clicked, action, &QAction::trigger);
        connect(action, &QAction::changed, button, [button, action]() {
            button->setText(action->text());
            button->setIcon(action->icon());
            button->setEnabled(action->isEnabled());
            button->setVisible(action->isVisible());
        });
        layout->insertWidget(stretchIndex, button);
        ++stretchIndex;
        _encryptionMessageButtons.insert(action, button);
    }
}

} // namespace OCC

#include "accountsettings.moc"
