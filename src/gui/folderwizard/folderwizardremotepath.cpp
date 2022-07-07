/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "folderwizardremotepath.h"
#include "ui_folderwizardtargetpage.h"

#include "folderwizard.h"
#include "folderwizard_p.h"

#include "gui/folderman.h"

#include "libsync/theme.h"

#include <QDir>
#include <QFileIconProvider>
#include <QInputDialog>

using namespace OCC;

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
    MkColJob *job = new MkColJob(_account, static_cast<FolderWizard *>(wizard())->d_func()->davUrl(), fullPath, {}, this);
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

const QString &FolderWizardRemotePath::targetPath() const
{
    return _targetPath;
}

void FolderWizardRemotePath::slotUpdateDirectories(const QStringList &list)
{
    QString webdavFolder = static_cast<FolderWizard *>(wizard())->d_func()->davUrl().path();

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
    LsColJob *job = new LsColJob(_account, static_cast<FolderWizard *>(wizard())->d_func()->davUrl(), path, this);
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
    const_cast<FolderWizardRemotePath *>(this)->_targetPath = dir;

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

    showWarn(FolderWizardPrivate::formatWarnings(warnStrings, !ok));
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
