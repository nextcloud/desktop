/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#include "folderwizard.h"

#include "ui_folderwizardsourcepage.h"
#include "ui_folderwizardtargetpage.h"

#include "spacespage.h"

#include "account.h"
#include "common/asserts.h"
#include "configfile.h"
#include "creds/abstractcredentials.h"
#include "gui/application.h"
#include "gui/askexperimentalvirtualfilesfeaturemessagebox.h"
#include "gui/settingsdialog.h"
#include "networkjobs.h"
#include "theme.h"

#include "gui/accountstate.h"
#include "gui/folderman.h"
#include "gui/selectivesyncdialog.h"
#include "gui/spaces/spacesmodel.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QValidator>
#include <QWizardPage>

#include <stdlib.h>

namespace {
QString formatWarnings(const QStringList &warnings, bool isError = false)
{
    QString ret;
    if (warnings.count() == 1) {
        ret = isError ? QApplication::translate("FolderWizard", "<b>Error:</b> %1").arg(warnings.first()) : QApplication::translate("FolderWizard", "<b>Warning:</b> %1").arg(warnings.first());
    } else if (warnings.count() > 1) {
        QStringList w2;
        for (const auto &warning : warnings) {
            w2.append(QStringLiteral("<li>%1</li>").arg(warning));
        }
        ret = isError ? QApplication::translate("FolderWizard", "<b>Error:</b><ul>%1</ul>").arg(w2.join(QString()))
                      : QApplication::translate("FolderWizard", "<b>Warning:</b><ul>%1</ul>").arg(w2.join(QString()));
    }

    return ret;
}
}


namespace OCC {

Q_LOGGING_CATEGORY(lcFolderWizard, "gui.folderwizard", QtInfoMsg)


FolderWizardLocalPath::FolderWizardLocalPath(const AccountPtr &account, QWidget *parent)
    : QWizardPage(parent)
    , _ui(new Ui_FolderWizardSourcePage)
    , _account(account)
{
    _ui->setupUi(this);
    registerField(QLatin1String("sourceFolder*"), _ui->localFolderLineEdit);
    connect(_ui->localFolderChooseBtn, &QAbstractButton::clicked, this, &FolderWizardLocalPath::slotChooseLocalFolder);
    _ui->localFolderChooseBtn->setToolTip(tr("Click to select a local folder to sync."));

    _ui->localFolderLineEdit->setToolTip(tr("Enter the path to the local folder."));

    _ui->warnLabel->setTextFormat(Qt::RichText);
    _ui->warnLabel->hide();
}

FolderWizardLocalPath::~FolderWizardLocalPath()
{
    delete _ui;
}

void FolderWizardLocalPath::initializePage()
{
    _ui->warnLabel->hide();
    _ui->localFolderLineEdit->setText(QDir::toNativeSeparators(static_cast<FolderWizard *>(wizard())->destination()));
}

void FolderWizardLocalPath::cleanupPage()
{
    _ui->warnLabel->hide();
}

bool FolderWizardLocalPath::isComplete() const
{
    QString errorStr = FolderMan::instance()->checkPathValidityForNewFolder(
        QDir::fromNativeSeparators(_ui->localFolderLineEdit->text()));


    bool isOk = errorStr.isEmpty();
    QStringList warnStrings;
    if (!isOk) {
        warnStrings << errorStr;
    }

    _ui->warnLabel->setWordWrap(true);
    if (isOk) {
        _ui->warnLabel->hide();
        _ui->warnLabel->clear();
    } else {
        _ui->warnLabel->show();
        QString warnings = formatWarnings(warnStrings);
        _ui->warnLabel->setText(warnings);
    }
    return isOk;
}

void FolderWizardLocalPath::slotChooseLocalFolder()
{
    QString sf = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QDir d(sf);

    // open the first entry of the home dir. Otherwise the dir picker comes
    // up with the closed home dir icon, stupid Qt default...
    QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
        QDir::DirsFirst | QDir::Name);

    if (dirs.count() > 0)
        sf += QLatin1Char('/') + dirs.at(0); // Take the first dir in home dir.

    QString dir = QFileDialog::getExistingDirectory(this,
        tr("Select the source folder"),
        sf);
    if (!dir.isEmpty()) {
        // set the last directory component name as alias
        _ui->localFolderLineEdit->setText(QDir::toNativeSeparators(dir));
    }
    emit completeChanged();
}

// =================================================================================
FolderWizardRemotePath::FolderWizardRemotePath(const AccountPtr &account, QWidget *parent)
    : QWizardPage(parent)
    , _ui(new Ui_FolderWizardTargetPage)
    , _warnWasVisible(false)
    , _account(account)

{
    _ui->setupUi(this);
    _ui->warnFrame->hide();

    _ui->folderTreeWidget->setSortingEnabled(true);
    _ui->folderTreeWidget->sortByColumn(0, Qt::AscendingOrder);

    connect(_ui->addFolderButton, &QAbstractButton::clicked, this, &FolderWizardRemotePath::slotAddRemoteFolder);
    connect(_ui->refreshButton, &QAbstractButton::clicked, this, &FolderWizardRemotePath::slotRefreshFolders);
    connect(_ui->folderTreeWidget, &QTreeWidget::itemExpanded, this, &FolderWizardRemotePath::slotItemExpanded);
    connect(_ui->folderTreeWidget, &QTreeWidget::currentItemChanged, this, &FolderWizardRemotePath::slotCurrentItemChanged);
    connect(_ui->folderEntry, &QLineEdit::textEdited, this, &FolderWizardRemotePath::slotFolderEntryEdited);

    _lscolTimer.setInterval(500);
    _lscolTimer.setSingleShot(true);
    connect(&_lscolTimer, &QTimer::timeout, this, &FolderWizardRemotePath::slotLsColFolderEntry);

    _ui->folderTreeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    // Make sure that there will be a scrollbar when the contents is too wide
    _ui->folderTreeWidget->header()->setStretchLastSection(false);
}

void FolderWizardRemotePath::slotAddRemoteFolder()
{
    QTreeWidgetItem *current = _ui->folderTreeWidget->currentItem();

    QString parent(QLatin1Char('/'));
    if (current) {
        parent = current->data(0, Qt::UserRole).toString();
    }

    QInputDialog *dlg = new QInputDialog(this);

    dlg->setWindowTitle(tr("Create Remote Folder"));
    dlg->setLabelText(tr("Enter the name of the new folder to be created below '%1':")
                          .arg(parent));
    dlg->open(this, SLOT(slotCreateRemoteFolder(QString)));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
}

void FolderWizardRemotePath::slotCreateRemoteFolder(const QString &folder)
{
    if (folder.isEmpty())
        return;

    QTreeWidgetItem *current = _ui->folderTreeWidget->currentItem();
    QString fullPath;
    if (current) {
        fullPath = current->data(0, Qt::UserRole).toString();
    }
    fullPath += QLatin1Char('/') + folder;

    // TODO: legacy
    MkColJob *job = new MkColJob(_account, static_cast<FolderWizard *>(wizard())->davUrl(), fullPath, {}, this);
    /* check the owncloud configuration file and query the ownCloud */
    connect(job, &MkColJob::finishedWithoutError,
        this, &FolderWizardRemotePath::slotCreateRemoteFolderFinished);
    connect(job, &AbstractNetworkJob::networkError, this, &FolderWizardRemotePath::slotHandleMkdirNetworkError);
    job->start();
}

void FolderWizardRemotePath::slotCreateRemoteFolderFinished()
{
    qCDebug(lcFolderWizard) << "webdav mkdir request finished";
    showWarn(tr("Folder was successfully created on %1.").arg(Theme::instance()->appNameGUI()));
    slotRefreshFolders();
    _ui->folderEntry->setText(static_cast<MkColJob *>(sender())->path());
    slotLsColFolderEntry();
}

void FolderWizardRemotePath::slotHandleMkdirNetworkError(QNetworkReply *reply)
{
    qCWarning(lcFolderWizard) << "webdav mkdir request failed:" << reply->error();
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
    OC_ASSERT(job);
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
    if (parentPath == QLatin1Char('/')) {
        folderPath = folderName;
    } else {
        folderPath = parentPath + QLatin1Char('/') + folderName;
    }
    QTreeWidgetItem *item = findFirstChild(parent, folderName);
    if (!item) {
        item = new QTreeWidgetItem(parent);
        QFileIconProvider prov;
        QIcon folderIcon = prov.icon(QFileIconProvider::Folder);
        item->setIcon(0, folderIcon);
        item->setText(0, folderName);
        item->setData(0, Qt::UserRole, folderPath);
        item->setToolTip(0, folderPath);
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

    QTreeWidgetItem *it = _ui->folderTreeWidget->topLevelItem(0);
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

    _ui->folderTreeWidget->setCurrentItem(it);
    _ui->folderTreeWidget->scrollToItem(it);
    return true;
}

void FolderWizardRemotePath::slotUpdateDirectories(const QStringList &list)
{
    QString webdavFolder = static_cast<FolderWizard *>(wizard())->davUrl().path();

    QTreeWidgetItem *root = _ui->folderTreeWidget->topLevelItem(0);
    if (!root) {
        root = new QTreeWidgetItem(_ui->folderTreeWidget);
        root->setText(0, Theme::instance()->appNameGUI());
        root->setIcon(0, Theme::instance()->applicationIcon());
        root->setToolTip(0, tr("Choose this to sync the entire account"));
        root->setData(0, Qt::UserRole, QStringLiteral("/"));
    }
    QStringList sortedList = list;
    Utility::sortFilenames(sortedList);
    for (auto path : qAsConst(sortedList)) {
        path.remove(webdavFolder);
        QStringList paths = path.split(QLatin1Char('/'));
        if (paths.last().isEmpty())
            paths.removeLast();
        recursiveInsert(root, paths, path);
    }
    root->setExpanded(true);
}

void FolderWizardRemotePath::slotRefreshFolders()
{
    runLsColJob(QStringLiteral("/"));
    _ui->folderTreeWidget->clear();
    _ui->folderEntry->clear();
}

void FolderWizardRemotePath::slotItemExpanded(QTreeWidgetItem *item)
{
    QString dir = item->data(0, Qt::UserRole).toString();
    if (!dir.startsWith(QLatin1Char('/'))) {
        dir.prepend(QLatin1Char('/'));
    }
    runLsColJob(dir);
}

void FolderWizardRemotePath::slotCurrentItemChanged(QTreeWidgetItem *item)
{
    if (item) {
        QString dir = item->data(0, Qt::UserRole).toString();
        if (!dir.startsWith(QLatin1Char('/'))) {
            dir.prepend(QLatin1Char('/'));
        }
        _ui->folderEntry->setText(dir);
    }

    emit completeChanged();
}

void FolderWizardRemotePath::slotFolderEntryEdited(const QString &text)
{
    if (selectByPath(text)) {
        _lscolTimer.stop();
        return;
    }

    _ui->folderTreeWidget->setCurrentItem(nullptr);
    _lscolTimer.start(); // avoid sending a request on each keystroke
}

void FolderWizardRemotePath::slotLsColFolderEntry()
{
    QString path = _ui->folderEntry->text();

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
    selectByPath(_ui->folderEntry->text());
}

LsColJob *FolderWizardRemotePath::runLsColJob(const QString &path)
{
    LsColJob *job = new LsColJob(_account, static_cast<FolderWizard *>(wizard())->davUrl(), path, this);
    job->setProperties(QList<QByteArray>() << "resourcetype");
    connect(job, &LsColJob::directoryListingSubfolders,
        this, &FolderWizardRemotePath::slotUpdateDirectories);
    connect(job, &LsColJob::finishedWithError,
        this, &FolderWizardRemotePath::slotHandleLsColNetworkError);
    job->start();

    return job;
}

FolderWizardRemotePath::~FolderWizardRemotePath()
{
    delete _ui;
}

bool FolderWizardRemotePath::isComplete() const
{
    if (!_ui->folderTreeWidget->currentItem())
        return false;

    QStringList warnStrings;
    QString dir = _ui->folderTreeWidget->currentItem()->data(0, Qt::UserRole).toString();
    if (!dir.startsWith(QLatin1Char('/'))) {
        dir.prepend(QLatin1Char('/'));
    }
    wizard()->setProperty("targetPath", dir);


    bool ok = true;

    for (auto *f : qAsConst(FolderMan::instance()->folders())) {
        if (f->accountState()->account() != _account) {
            continue;
        }
        QString curDir = f->remotePathTrailingSlash();
        if (QDir::cleanPath(dir) == QDir::cleanPath(curDir)) {
            if (Theme::instance()->allowDuplicatedFolderSyncPair()) {
                warnStrings.append(tr("This folder is already being synced."));
            } else {
                ok = false;
                warnStrings.append(tr("This folder can't be synced. Please choose another one."));
            }
        } else if (dir.startsWith(curDir)) {
            warnStrings.append(tr("You are already syncing <i>%1</i>, which is a parent folder of <i>%2</i>.").arg(Utility::escape(curDir), Utility::escape(dir)));
        } else if (curDir.startsWith(dir)) {
            warnStrings.append(tr("You are already syncing <i>%1</i>, which is a subfolder of <i>%2</i>.").arg(Utility::escape(curDir), Utility::escape(dir)));
        }
    }

    showWarn(formatWarnings(warnStrings, !ok));
    return ok;
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
        _ui->warnFrame->hide();

    } else {
        _ui->warnFrame->show();
        _ui->warnLabel->setText(msg);
    }
}

// ====================================================================================

FolderWizardSelectiveSync::FolderWizardSelectiveSync(const AccountPtr &account, QWidget *parent)
    : QWizardPage(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    _selectiveSync = new SelectiveSyncWidget(account, this);
    layout->addWidget(_selectiveSync);

    const auto vfsMode = bestAvailableVfsMode();
    if (Theme::instance()->forceVirtualFilesOption() && vfsMode == Vfs::WindowsCfApi) {
        // using an else if to make the condition not even more complex...
    } else if (Theme::instance()->showVirtualFilesOption() && vfsMode != Vfs::Off && (vfsMode == Vfs::WindowsCfApi || Theme::instance()->enableExperimentalFeatures())) {
        _virtualFilesCheckBox = new QCheckBox(tr("Use virtual files instead of downloading content immediately%1").arg(vfsMode == Vfs::WindowsCfApi ? QString() : tr(" (experimental)")));
        connect(_virtualFilesCheckBox, &QCheckBox::clicked, this, &FolderWizardSelectiveSync::virtualFilesCheckboxClicked);
        connect(_virtualFilesCheckBox, &QCheckBox::stateChanged, this, [this](int state) {
            _selectiveSync->setEnabled(state == Qt::Unchecked);
        });
        _virtualFilesCheckBox->setChecked(vfsMode == Vfs::WindowsCfApi);
        layout->addWidget(_virtualFilesCheckBox);
    }
}

FolderWizardSelectiveSync::~FolderWizardSelectiveSync()
{
}


void FolderWizardSelectiveSync::initializePage()
{
    QString targetPath = wizard()->property("targetPath").toString();
    QString alias = QFileInfo(targetPath).fileName();
    if (alias.isEmpty())
        alias = Theme::instance()->appName();
    _selectiveSync->setFolderInfo(targetPath, alias);
    QWizardPage::initializePage();
}

bool FolderWizardSelectiveSync::validatePage()
{
    wizard()->setProperty("selectiveSyncBlackList", useVirtualFiles() ? QVariant() : QVariant(_selectiveSync->createBlackList()));
    return true;
}

bool FolderWizardSelectiveSync::useVirtualFiles() const
{
    return _virtualFilesCheckBox && _virtualFilesCheckBox->isChecked();
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
        auto *messageBox = new AskExperimentalVirtualFilesFeatureMessageBox(this);

        connect(messageBox, &AskExperimentalVirtualFilesFeatureMessageBox::rejected, this, [this]() {
            _virtualFilesCheckBox->setChecked(false);
        });

        // no need to show the message box on Windows
        // as a little shortcut, we just re-use the message box's accept handler
        if (bestAvailableVfsMode() == Vfs::WindowsCfApi) {
            Q_EMIT messageBox->accepted();
        } else {
            messageBox->show();
        }
    }
}


// ====================================================================================


/**
 * Folder wizard itself
 */

FolderWizard::FolderWizard(AccountPtr account, QWidget *parent, Qt::WindowFlags flags)
    : QWizard(parent, flags)
    , _account(account)
    , _folderWizardSourcePage(new FolderWizardLocalPath(account))
    , _folderWizardSelectiveSyncPage(new FolderWizardSelectiveSync(account))
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    if (account->capabilities().spacesSupport().enabled) {
        _spacesPage = new SpacesPage(account, this);
        setPage(Page_Space, _spacesPage);
        _spacesPage->installEventFilter(this);
    }
    setPage(Page_Source, _folderWizardSourcePage);
    _folderWizardSourcePage->installEventFilter(this);
    // for now spaces are meant to be synced as a whole
    if (!account->capabilities().spacesSupport().enabled && !Theme::instance()->singleSyncFolder()) {
        _folderWizardTargetPage = new FolderWizardRemotePath(account);
        setPage(Page_Target, _folderWizardTargetPage);
        _folderWizardTargetPage->installEventFilter(this);
    }
    if (!account->capabilities().spacesSupport().enabled) {
        // TODO: add spaces support to selective sync
        setPage(Page_SelectiveSync, _folderWizardSelectiveSyncPage);
    }

    setWindowTitle(tr("Add Folder Sync Connection"));
    setOptions(QWizard::CancelButtonOnLeft);
    setButtonText(QWizard::FinishButton, tr("Add Sync Connection"));
    setWizardStyle(QWizard::ModernStyle);
}

FolderWizard::~FolderWizard()
{
}

QUrl FolderWizard::davUrl() const
{
    if (_account->capabilities().spacesSupport().enabled) {
        auto url = _spacesPage->selectedSpace(Spaces::SpacesModel::Columns::WebDavUrl).toUrl();
        if (!url.path().endsWith(QLatin1Char('/'))) {
            url.setPath(url.path() + QLatin1Char('/'));
        }
        return url;
    }
    return _account->davUrl();
}

QString FolderWizard::destination() const
{
    if (!_account->hasDefaultSyncRoot()) {
        _account->setDefaultSyncRoot(FolderMan::suggestSyncFolder(_account->url(), _account->davDisplayName()));
    }
    QString defaultPath = _account->defaultSyncRoot();
    if (_account->capabilities().spacesSupport().enabled) {
        defaultPath += QDir::separator() + _spacesPage->selectedSpace(Spaces::SpacesModel::Columns::LocalMountPoint).toString();
        FolderMan::prepareFolder(defaultPath);
        defaultPath += QDir::separator() + _spacesPage->selectedSpace(Spaces::SpacesModel::Columns::Name).toString();
    };
    return FolderMan::instance()->findGoodPathForNewSyncFolder(defaultPath);
}


QString FolderWizard::displayName() const
{
    if (_account->capabilities().spacesSupport().enabled) {
        return _spacesPage->selectedSpace(Spaces::SpacesModel::Columns::Name).toString();
    };
    return QString();
}

bool FolderWizard::useVirtualFiles() const
{
    const auto mode = bestAvailableVfsMode();
    const bool useVirtualFiles = (Theme::instance()->forceVirtualFilesOption() && mode == Vfs::WindowsCfApi) || (_folderWizardSelectiveSyncPage->useVirtualFiles());
    if (useVirtualFiles) {
        const auto availability = Vfs::checkAvailability(destination(), mode);
        if (!availability) {
            auto msg = new QMessageBox(QMessageBox::Warning, tr("Virtual files are not available for the selected folder"), availability.error(), QMessageBox::Ok, ocApp()->gui()->settingsDialog());
            msg->setAttribute(Qt::WA_DeleteOnClose);
            msg->open();
            return false;
        }
    }
    return useVirtualFiles;
}

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
