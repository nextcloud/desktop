/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "folderwizard.h"
#include "folderman.h"
#include "configfile.h"
#include "theme.h"
#include "networkjobs.h"
#include "account.h"
#include "selectivesyncdialog.h"
#include "accountstate.h"
#include "creds/abstractcredentials.h"
#include "wizard/owncloudwizard.h"
#include "common/asserts.h"

#ifdef Q_OS_MACOS
#include "common/utility_mac_sandbox.h"
#endif

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QInputDialog>
#include <QUrl>
#include <QValidator>
#include <QWizardPage>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QEvent>
#include <QCheckBox>
#include <QMessageBox>
#include <QStandardPaths>

#include <cstdlib>

namespace
{
constexpr QColor darkWarnYellow(63, 63, 0);
constexpr QColor lightWarnYellow(255, 255, 192);

QPalette yellowWarnWidgetPalette(const QPalette &existingPalette)
{
    const auto warnYellow = OCC::Theme::instance()->darkMode() ? darkWarnYellow : lightWarnYellow;
    auto modifiedPalette = existingPalette;
    modifiedPalette.setColor(QPalette::Window, warnYellow);
    modifiedPalette.setColor(QPalette::Base, warnYellow);
    return modifiedPalette;
}
}

namespace OCC {

QString FormatWarningsWizardPage::formatWarnings(const QStringList &warnings) const
{
    QString formattedWarning;
    if (warnings.count() == 1) {
        formattedWarning = Utility::escape(warnings.first());
    } else if (warnings.count() > 1) {
        formattedWarning = "<ul>";
        for (const auto &warning : warnings) {
            formattedWarning += QString::fromLatin1("<li>%1</li>").arg(Utility::escape(warning));
        }
        formattedWarning += "</ul>";
    }

    return formattedWarning;
}

FolderWizardLocalPath::FolderWizardLocalPath(const AccountPtr &account)
    : FormatWarningsWizardPage()
    , _account(account)
{
    _ui.setupUi(this);
    registerField(QLatin1String("sourceFolder*"), _ui.localFolderLineEdit);
    connect(_ui.localFolderChooseBtn, &QAbstractButton::clicked, this, &FolderWizardLocalPath::slotChooseLocalFolder);
    _ui.localFolderChooseBtn->setToolTip(tr("Click to select a local folder to sync."));

    QUrl serverUrl = _account->url();
    serverUrl.setUserName(_account->credentials()->user());
    _ui.localFolderLineEdit->setToolTip(tr("Enter the path to the local folder."));

    _ui.warnLabel->setTextFormat(Qt::RichText);
    _ui.warnLabel->hide();

    changeStyle();
}

FolderWizardLocalPath::~FolderWizardLocalPath() = default;

void FolderWizardLocalPath::initializePage()
{
    _ui.warnLabel->hide();
    
    // Automatically trigger folder selection dialog on first appearance
    if (_initialFolderSelection) {
        // Use QTimer to defer the dialog until the page is fully shown
        QTimer::singleShot(0, this, &FolderWizardLocalPath::slotChooseLocalFolder);
    }
}

void FolderWizardLocalPath::cleanupPage()
{
    _ui.warnLabel->hide();
}

bool FolderWizardLocalPath::isComplete() const
{
    QUrl serverUrl = _account->url();
    serverUrl.setUserName(_account->credentials()->user());

    const auto errorStr = FolderMan::instance()->checkPathValidityForNewFolder(
        QDir::fromNativeSeparators(_ui.localFolderLineEdit->text()), serverUrl).second;


    bool isOk = errorStr.isEmpty();
    QStringList warnStrings;
    if (!isOk) {
        warnStrings << errorStr;
    }

    _ui.warnLabel->setWordWrap(true);
    if (isOk) {
        _ui.warnLabel->hide();
        _ui.warnLabel->clear();
    } else {
        _ui.warnLabel->show();
        QString warnings = formatWarnings(warnStrings);
        _ui.warnLabel->setText(warnings);
    }
    return isOk;
}

void FolderWizardLocalPath::slotChooseLocalFolder()
{
    const bool isInitialSelection = _initialFolderSelection;
    QString sf;

    #ifdef Q_OS_MACOS
        // On macOS with app sandbox, QStandardPaths returns the sandbox container directory,
        // not the actual user home directory. Use NSHomeDirectory() to get the real path.
        sf = Utility::getRealHomeDirectory();
    #else
        sf = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    #endif

    QString dir = QFileDialog::getExistingDirectory(this,
        tr("Select the source folder"),
        sf,
        QFileDialog::ShowDirsOnly);
    if (!dir.isEmpty()) {
        _ui.localFolderLineEdit->setText(QDir::toNativeSeparators(dir));
        _initialFolderSelection = false;
    } else {
        // If this was the initial folder selection and the user canceled,
        // emit signal to close the wizard
        if (isInitialSelection) {
            emit initialFolderSelectionCanceled();
        }
    }
    emit completeChanged();
}


void FolderWizardLocalPath::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ThemeChange:
        // Notify the other widgets (Dark-/Light-Mode switching)
        changeStyle();
        break;
    default:
        break;
    }

    FormatWarningsWizardPage::changeEvent(e);
}

void FolderWizardLocalPath::changeStyle()
{
    const auto yellowWarnPalette = yellowWarnWidgetPalette(_ui.warnLabel->palette());
    _ui.warnLabel->setPalette(yellowWarnPalette);
}

// =================================================================================
FolderWizardRemotePath::FolderWizardRemotePath(const AccountPtr &account)
    : FormatWarningsWizardPage()
    , _account(account)
{
    _ui.setupUi(this);
    _ui.warnFrame->hide();

    _ui.folderTreeWidget->setSortingEnabled(true);
    _ui.folderTreeWidget->sortByColumn(0, Qt::AscendingOrder);

    connect(_ui.addFolderButton, &QAbstractButton::clicked, this, &FolderWizardRemotePath::slotAddRemoteFolder);
    connect(_ui.refreshButton, &QAbstractButton::clicked, this, &FolderWizardRemotePath::slotRefreshFolders);
    connect(_ui.folderTreeWidget, &QTreeWidget::itemExpanded, this, &FolderWizardRemotePath::slotItemExpanded);
    connect(_ui.folderTreeWidget, &QTreeWidget::currentItemChanged, this, &FolderWizardRemotePath::slotCurrentItemChanged);
    connect(_ui.folderEntry, &QLineEdit::textEdited, this, &FolderWizardRemotePath::slotFolderEntryEdited);

    _lscolTimer.setInterval(500);
    _lscolTimer.setSingleShot(true);
    connect(&_lscolTimer, &QTimer::timeout, this, &FolderWizardRemotePath::slotLsColFolderEntry);

    _ui.folderTreeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    // Make sure that there will be a scrollbar when the contents is too wide
    _ui.folderTreeWidget->header()->setStretchLastSection(false);

    changeStyle();
}

void FolderWizardRemotePath::slotAddRemoteFolder()
{
    QTreeWidgetItem *current = _ui.folderTreeWidget->currentItem();

    QString parent('/');
    if (current) {
        parent = current->data(0, Qt::UserRole).toString();
    }

    auto *dlg = new QInputDialog(this);

    dlg->setWindowTitle(tr("Create Remote Folder"));
    dlg->setLabelText(tr("Enter the name of the new folder to be created below \"%1\":")
                          .arg(parent));
    dlg->open(this, SLOT(slotCreateRemoteFolder(QString)));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
}

void FolderWizardRemotePath::slotCreateRemoteFolder(const QString &folder)
{
    if (folder.isEmpty())
        return;

    QTreeWidgetItem *current = _ui.folderTreeWidget->currentItem();
    QString fullPath;
    if (current) {
        fullPath = current->data(0, Qt::UserRole).toString();
    }
    fullPath += "/" + folder;

    auto *job = new MkColJob(_account, fullPath, this);
    /* check the owncloud configuration file and query the ownCloud */
    connect(job, &MkColJob::finishedWithoutError,
        this, &FolderWizardRemotePath::slotCreateRemoteFolderFinished);
    connect(job, &AbstractNetworkJob::networkError, this, &FolderWizardRemotePath::slotHandleMkdirNetworkError);
    job->start();
}

void FolderWizardRemotePath::slotCreateRemoteFolderFinished()
{
    qCDebug(lcWizard) << "webdav mkdir request finished";
    showWarn(tr("Folder was successfully created on %1.").arg(Theme::instance()->appNameGUI()));
    slotRefreshFolders();
    _ui.folderEntry->setText(dynamic_cast<MkColJob *>(sender())->path());
    slotLsColFolderEntry();
}

void FolderWizardRemotePath::slotHandleMkdirNetworkError(QNetworkReply *reply)
{
    qCWarning(lcWizard) << "webdav mkdir request failed:" << reply->error();
    if (!_account->credentials()->stillValid(reply)) {
        showWarn(tr("Authentication failed accessing %1").arg(Theme::instance()->appNameGUI()));
    } else {
        showWarn(tr("Failed to create the folder on %1. Please check manually.")
                     .arg(Theme::instance()->appNameGUI()));
    }
}

void FolderWizardRemotePath::slotHandleLsColNetworkError(QNetworkReply *reply)
{
    // Ignore 404s, otherwise users will get annoyed by error popups
    // when not typing fast enough. It's still clear that a given path
    // was not found, because the 'Next' button is disabled and no entry
    // is selected in the tree view.
    int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpCode == 404) {
        showWarn(QString()); // hides the warning pane
        return;
    }
    auto job = qobject_cast<LsColJob *>(sender());
    ASSERT(job);
    showWarn(tr("Failed to list a folder. Error: %1")
                 .arg(job->errorStringParsingBody()));
}

static QTreeWidgetItem *findFirstChild(QTreeWidgetItem *parent, const QString &text)
{
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        if (child->text(0) == text) {
            return child;
        }
    }
    return nullptr;
}

void FolderWizardRemotePath::recursiveInsert(QTreeWidgetItem *parent, QStringList pathTrail, QString path)
{
    if (pathTrail.isEmpty())
        return;

    const QString parentPath = parent->data(0, Qt::UserRole).toString();
    const QString folderName = pathTrail.first();
    QString folderPath;
    if (parentPath == QLatin1String("/")) {
        folderPath = folderName;
    } else {
        folderPath = parentPath + "/" + folderName;
    }
    QTreeWidgetItem *item = findFirstChild(parent, folderName);
    if (!item) {
        item = new QTreeWidgetItem(parent);
        QFileIconProvider prov;
        QIcon folderIcon = prov.icon(QFileIconProvider::Folder);
        item->setIcon(0, folderIcon);
        item->setText(0, folderName);
        item->setData(0, Qt::UserRole, folderPath);
        item->setToolTip(0, Utility::escape(folderPath));
        item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    }

    pathTrail.removeFirst();
    recursiveInsert(item, pathTrail, path);
}

bool FolderWizardRemotePath::selectByPath(QString path)
{
    if (path.startsWith(QLatin1Char('/'))) {
        path = path.mid(1);
    }
    if (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }

    QTreeWidgetItem *it = _ui.folderTreeWidget->topLevelItem(0);
    if (!path.isEmpty()) {
        const QStringList pathTrail = path.split(QLatin1Char('/'));
        for (const auto &path : pathTrail) {
            if (!it) {
                return false;
            }
            it = findFirstChild(it, path);
        }
    }
    if (!it) {
        return false;
    }

    _ui.folderTreeWidget->setCurrentItem(it);
    _ui.folderTreeWidget->scrollToItem(it);
    return true;
}

void FolderWizardRemotePath::slotUpdateDirectories(const QStringList &list)
{
    QString webdavFolder = QUrl(_account->davUrl()).path();

    QTreeWidgetItem *root = _ui.folderTreeWidget->topLevelItem(0);
    if (!root) {
        root = new QTreeWidgetItem(_ui.folderTreeWidget);
        root->setText(0, Theme::instance()->appNameGUI());
        root->setIcon(0, Theme::instance()->applicationIcon());
        root->setToolTip(0, tr("Choose this to sync the entire account"));
        root->setData(0, Qt::UserRole, "/");
    }
    QStringList sortedList = list;
    Utility::sortFilenames(sortedList);
    for (auto path : std::as_const(sortedList)) {
        path.remove(webdavFolder);

        // Don't allow to select subfolders of encrypted subfolders
        const auto isAnyAncestorEncrypted = std::any_of(std::cbegin(_encryptedPaths), std::cend(_encryptedPaths), [=](const QString &encryptedPath) {
            return path.size() > encryptedPath.size() && path.startsWith(encryptedPath);
        });
        if (isAnyAncestorEncrypted) {
            continue;
        }

        QStringList paths = path.split('/');
        if (paths.last().isEmpty())
            paths.removeLast();
        recursiveInsert(root, paths, path);
    }
    root->setExpanded(true);
}

void FolderWizardRemotePath::slotGatherEncryptedPaths(const QString &path, const QMap<QString, QString> &properties)
{
    const auto it = properties.find("is-encrypted");
    if (it == properties.cend() || *it != QStringLiteral("1")) {
        return;
    }

    const auto webdavFolder = QUrl(_account->davUrl()).path();
    Q_ASSERT(path.startsWith(webdavFolder));
    _encryptedPaths << path.mid(webdavFolder.size());
}

void FolderWizardRemotePath::slotRefreshFolders()
{
    _encryptedPaths.clear();
    runLsColJob("/");
    _ui.folderTreeWidget->clear();
    _ui.folderEntry->clear();
}

void FolderWizardRemotePath::slotItemExpanded(QTreeWidgetItem *item)
{
    QString dir = item->data(0, Qt::UserRole).toString();
    runLsColJob(dir);
}

void FolderWizardRemotePath::slotCurrentItemChanged(QTreeWidgetItem *item)
{
    if (item) {
        QString dir = item->data(0, Qt::UserRole).toString();

        // We don't want to allow creating subfolders in encrypted folders outside of the sync logic
        const auto encrypted = _encryptedPaths.contains(dir);
        _ui.addFolderButton->setEnabled(!encrypted);

        if (!dir.startsWith(QLatin1Char('/'))) {
            dir.prepend(QLatin1Char('/'));
        }
        _ui.folderEntry->setText(dir);
    }

    emit completeChanged();
}

void FolderWizardRemotePath::slotFolderEntryEdited(const QString &text)
{
    if (selectByPath(text)) {
        _lscolTimer.stop();
        return;
    }

    _ui.folderTreeWidget->setCurrentItem(nullptr);
    _lscolTimer.start(); // avoid sending a request on each keystroke
}

void FolderWizardRemotePath::slotLsColFolderEntry()
{
    QString path = _ui.folderEntry->text();
    if (path.startsWith(QLatin1Char('/')))
        path = path.mid(1);

    LsColJob *job = runLsColJob(path);
    // No error handling, no updating, we do this manually
    // because of extra logic in the typed-path case.
    disconnect(job, nullptr, this, nullptr);
    connect(job, &LsColJob::finishedWithError,
        this, &FolderWizardRemotePath::slotHandleLsColNetworkError);
    connect(job, &LsColJob::directoryListingSubfolders,
        this, &FolderWizardRemotePath::slotTypedPathFound);
}

void FolderWizardRemotePath::slotTypedPathFound(const QStringList &subpaths)
{
    slotUpdateDirectories(subpaths);
    selectByPath(_ui.folderEntry->text());
}

LsColJob *FolderWizardRemotePath::runLsColJob(const QString &path)
{
    auto *job = new LsColJob(_account, path);
    const auto props = QList<QByteArray>() << "resourcetype"
                                           << "http://nextcloud.org/ns:is-encrypted";
    job->setProperties(props);
    connect(job, &LsColJob::directoryListingSubfolders,
        this, &FolderWizardRemotePath::slotUpdateDirectories);
    connect(job, &LsColJob::finishedWithError,
        this, &FolderWizardRemotePath::slotHandleLsColNetworkError);
    connect(job, &LsColJob::directoryListingIterated,
        this, &FolderWizardRemotePath::slotGatherEncryptedPaths);
    job->start();

    return job;
}

FolderWizardRemotePath::~FolderWizardRemotePath() = default;

bool FolderWizardRemotePath::isComplete() const
{
    if (!_ui.folderTreeWidget->currentItem()) {
        return false;
    }

    auto targetPath = _ui.folderTreeWidget->currentItem()->data(0, Qt::UserRole).toString();
    if (!targetPath.startsWith(QLatin1Char('/'))) {
        targetPath.prepend(QLatin1Char('/'));
    }
    wizard()->setProperty("targetPath", targetPath);

    for (const auto folder : std::as_const(FolderMan::instance()->map())) {
        if (folder->accountState()->account() != _account) {
            continue;
        }

        const auto remoteDir = folder->remotePathTrailingSlash();
        const auto localDir = folder->cleanPath();
        if (QDir::cleanPath(targetPath) == QDir::cleanPath(remoteDir)) {
            showWarn(tr("Please choose a different location. %1 is already being synced to %2.").arg(Utility::escape(remoteDir), Utility::escape(localDir)));
            break;
        }

        if (targetPath.startsWith(remoteDir)) {
            _ui.warnFrame->show();
            _ui.warnLabel->hide();
            _ui.infoLabel->setText(tr("You are already syncing the subfolder %1 at %2.").arg(Utility::escape(targetPath), Utility::escape(localDir)));
            break;
        }

        if (remoteDir.startsWith(targetPath)) {
            showWarn(tr("Please choose a different location. %1 is already being synced to %2.").arg(Utility::escape(remoteDir), Utility::escape(localDir)));
            break;
        }
    }

    return true;
}

void FolderWizardRemotePath::cleanupPage()
{
    showWarn();
}

void FolderWizardRemotePath::initializePage()
{
    showWarn();
    slotRefreshFolders();
}

void FolderWizardRemotePath::showWarn(const QString &msg) const
{
    if (msg.isEmpty()) {
        _ui.warnFrame->hide();

    } else {
        _ui.warnFrame->show();
        _ui.infoLabel->hide();
        _ui.warnLabel->setText(msg);
    }
}

void FolderWizardRemotePath::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ThemeChange:
        // Notify the other widgets (Dark-/Light-Mode switching)
        changeStyle();
        break;
    default:
        break;
    }

    FormatWarningsWizardPage::changeEvent(e);
}

void FolderWizardRemotePath::changeStyle()
{
    const auto yellowWarnPalette = yellowWarnWidgetPalette(_ui.warnLabel->palette());
    _ui.warnLabel->setPalette(yellowWarnPalette);
}

// ====================================================================================

FolderWizardSelectiveSync::FolderWizardSelectiveSync(const AccountPtr &account)
{
    auto *layout = new QVBoxLayout(this);
    _selectiveSync = new SelectiveSyncWidget(account, this);
    layout->addWidget(_selectiveSync);

    if (!Theme::instance()->disableVirtualFilesSyncFolder() && Theme::instance()->showVirtualFilesOption() && bestAvailableVfsMode() != Vfs::Off) {
        _virtualFilesCheckBox = new QCheckBox(tr("Use virtual files instead of downloading content immediately %1").arg(bestAvailableVfsMode() == Vfs::WindowsCfApi ? QString() : tr("(experimental)")));
        connect(_virtualFilesCheckBox, &QCheckBox::clicked, this, &FolderWizardSelectiveSync::virtualFilesCheckboxClicked);
        connect(_virtualFilesCheckBox, &QCheckBox::checkStateChanged, this, [this](int state) {
            _selectiveSync->setEnabled(state == Qt::Unchecked);
        });
        _virtualFilesCheckBox->setChecked(bestAvailableVfsMode() == Vfs::WindowsCfApi);
        layout->addWidget(_virtualFilesCheckBox);
    }
}

FolderWizardSelectiveSync::~FolderWizardSelectiveSync() = default;


void FolderWizardSelectiveSync::initializePage()
{
    QString targetPath = wizard()->property("targetPath").toString();
    if (targetPath.startsWith('/')) {
        targetPath = targetPath.mid(1);
    }
    QString alias = QFileInfo(targetPath).fileName();
    if (alias.isEmpty())
        alias = Theme::instance()->appName();
    QStringList initialBlacklist;
    if (Theme::instance()->wizardSelectiveSyncDefaultNothing()) {
        initialBlacklist = QStringList("/");
    }
    _selectiveSync->setFolderInfo(targetPath, alias, initialBlacklist);

    if (_virtualFilesCheckBox) {
        // TODO: remove when UX decision is made
        if (Utility::isPathWindowsDrivePartitionRoot(wizard()->field(QStringLiteral("sourceFolder")).toString())) {
            _virtualFilesCheckBox->setChecked(false);
            _virtualFilesCheckBox->setEnabled(false);
            _virtualFilesCheckBox->setText(tr("Virtual files are not supported for Windows partition roots as local folder. Please choose a valid subfolder under drive letter."));
        } else {
            _virtualFilesCheckBox->setChecked(bestAvailableVfsMode() == Vfs::WindowsCfApi);
            _virtualFilesCheckBox->setEnabled(true);
            _virtualFilesCheckBox->setText(tr("Use virtual files instead of downloading content immediately %1").arg(bestAvailableVfsMode() == Vfs::WindowsCfApi ? QString() : tr("(experimental)")));

            if (Theme::instance()->enforceVirtualFilesSyncFolder()) {
                _virtualFilesCheckBox->setChecked(true);
                _virtualFilesCheckBox->setDisabled(true);
            }
        }
        //
    }

    QWizardPage::initializePage();
}

bool FolderWizardSelectiveSync::validatePage()
{
    const auto mode = bestAvailableVfsMode();
    const bool useVirtualFiles = (mode == Vfs::WindowsCfApi) && (_virtualFilesCheckBox && _virtualFilesCheckBox->isChecked());
    if (useVirtualFiles) {
        const auto availability = Vfs::checkAvailability(wizard()->field(QStringLiteral("sourceFolder")).toString(), mode);
        if (!availability) {
            auto msg = new QMessageBox(QMessageBox::Warning,
                                       tr("Virtual files are not supported at the selected location"),
                                       availability.error(),
                                       QMessageBox::Ok, this);
            msg->setAttribute(Qt::WA_DeleteOnClose);
            msg->open();
            return false;
        }
    }
    wizard()->setProperty("selectiveSyncBlackList", useVirtualFiles ? QVariant() : QVariant(_selectiveSync->createBlackList()));
    wizard()->setProperty("useVirtualFiles", QVariant(useVirtualFiles));
    return true;
}

void FolderWizardSelectiveSync::cleanupPage()
{
    QString targetPath = wizard()->property("targetPath").toString();
    QString alias = QFileInfo(targetPath).fileName();
    if (alias.isEmpty())
        alias = Theme::instance()->appName();
    _selectiveSync->setFolderInfo(targetPath, alias);
    QWizardPage::cleanupPage();
}

void FolderWizardSelectiveSync::virtualFilesCheckboxClicked()
{
    // The click has already had an effect on the box, so if it's
    // checked it was newly activated.
    if (_virtualFilesCheckBox->isChecked()) {
        OwncloudWizard::askExperimentalVirtualFilesFeature(this, [this](bool enable) {
            if (!enable)
                _virtualFilesCheckBox->setChecked(false);
        });
    }
}


// ====================================================================================


/**
 * Folder wizard itself
 */

FolderWizard::FolderWizard(AccountPtr account, QWidget *parent)
    : QWizard(parent)
    , _folderWizardSourcePage(new FolderWizardLocalPath(account))
    , _folderWizardSelectiveSyncPage(new FolderWizardSelectiveSync(account))
{
    setWizardStyle(QWizard::ModernStyle);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setPage(Page_Source, _folderWizardSourcePage);
    _folderWizardSourcePage->installEventFilter(this);
    if (!Theme::instance()->singleSyncFolder()) {
        _folderWizardTargetPage = new FolderWizardRemotePath(account);
        setPage(Page_Target, _folderWizardTargetPage);
        _folderWizardTargetPage->installEventFilter(this);
    }
    setPage(Page_SelectiveSync, _folderWizardSelectiveSyncPage);

    setWindowTitle(tr("Add Folder Sync Connection"));
    setOptions(QWizard::CancelButtonOnLeft);
    setButtonText(QWizard::FinishButton, tr("Add Sync Connection"));
    
    // Close the wizard if initial folder selection is canceled
    connect(_folderWizardSourcePage, &FolderWizardLocalPath::initialFolderSelectionCanceled,
            this, &FolderWizard::reject);
}

FolderWizard::~FolderWizard() = default;

bool FolderWizard::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::LayoutRequest) {
        // Workaround QTBUG-3396:  forces QWizardPrivate::updateLayout()
        QTimer::singleShot(0, this, [this] { setTitleFormat(titleFormat()); });
    }
    return QWizard::eventFilter(watched, event);
}

void FolderWizard::resizeEvent(QResizeEvent *event)
{
    QWizard::resizeEvent(event);

    // workaround for QTBUG-22819: when the error label word wrap, the minimum height is not adjusted
    if (auto page = currentPage()) {
        int hfw = page->heightForWidth(page->width());
        if (page->height() < hfw) {
            page->setMinimumSize(page->minimumSizeHint().width(), hfw);
            setTitleFormat(titleFormat()); // And another workaround for QTBUG-3396
        }
    }
}

} // end namespace
