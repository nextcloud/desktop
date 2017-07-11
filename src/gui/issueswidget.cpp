/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include <QtGui>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QtWidgets>
#endif

#include "issueswidget.h"
#include "configfile.h"
#include "syncresult.h"
#include "logger.h"
#include "utility.h"
#include "theme.h"
#include "folderman.h"
#include "syncfileitem.h"
#include "folder.h"
#include "openfilemanager.h"
#include "activityitemdelegate.h"
#include "protocolwidget.h"
#include "accountstate.h"
#include "account.h"
#include "accountmanager.h"
#include "syncjournalfilerecord.h"
#include "elidedlabel.h"

#include "ui_issueswidget.h"

#include <climits>

namespace OCC {

IssuesWidget::IssuesWidget(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::IssuesWidget)
{
    _ui->setupUi(this);

    connect(ProgressDispatcher::instance(), SIGNAL(progressInfo(QString, ProgressInfo)),
        this, SLOT(slotProgressInfo(QString, ProgressInfo)));
    connect(ProgressDispatcher::instance(), SIGNAL(itemCompleted(QString, SyncFileItemPtr)),
        this, SLOT(slotItemCompleted(QString, SyncFileItemPtr)));
    connect(ProgressDispatcher::instance(), &ProgressDispatcher::syncError,
        this, &IssuesWidget::addError);

    connect(_ui->_treeWidget, SIGNAL(itemActivated(QTreeWidgetItem *, int)), SLOT(slotOpenFile(QTreeWidgetItem *, int)));
    connect(_ui->copyIssuesButton, SIGNAL(clicked()), SIGNAL(copyToClipboard()));

    connect(_ui->showIgnores, SIGNAL(toggled(bool)), SLOT(slotRefreshIssues()));
    connect(_ui->showWarnings, SIGNAL(toggled(bool)), SLOT(slotRefreshIssues()));
    connect(_ui->filterAccount, SIGNAL(currentIndexChanged(int)), SLOT(slotRefreshIssues()));
    connect(_ui->filterAccount, SIGNAL(currentIndexChanged(int)), SLOT(slotUpdateFolderFilters()));
    connect(_ui->filterFolder, SIGNAL(currentIndexChanged(int)), SLOT(slotRefreshIssues()));
    for (auto account : AccountManager::instance()->accounts()) {
        slotAccountAdded(account.data());
    }
    connect(AccountManager::instance(), SIGNAL(accountAdded(AccountState *)),
        SLOT(slotAccountAdded(AccountState *)));
    connect(AccountManager::instance(), SIGNAL(accountRemoved(AccountState *)),
        SLOT(slotAccountRemoved(AccountState *)));
    connect(FolderMan::instance(), SIGNAL(folderListChanged(Folder::Map)),
        SLOT(slotUpdateFolderFilters()));


    // Adjust copyToClipboard() when making changes here!
    QStringList header;
    header << tr("Time");
    header << tr("File");
    header << tr("Folder");
    header << tr("Issue");

    int timestampColumnExtra = 0;
#ifdef Q_OS_WIN
    timestampColumnExtra = 20; // font metrics are broken on Windows, see #4721
#endif

    _ui->_treeWidget->setHeaderLabels(header);
    int timestampColumnWidth =
        ActivityItemDelegate::rowHeight() // icon
        + _ui->_treeWidget->fontMetrics().width(ProtocolWidget::timeString(QDateTime::currentDateTime()))
        + timestampColumnExtra;
    _ui->_treeWidget->setColumnWidth(0, timestampColumnWidth);
    _ui->_treeWidget->setColumnWidth(1, 180);
    _ui->_treeWidget->setColumnCount(4);
    _ui->_treeWidget->setRootIsDecorated(false);
    _ui->_treeWidget->setTextElideMode(Qt::ElideMiddle);
    _ui->_treeWidget->header()->setObjectName("ActivityErrorListHeader");
#if defined(Q_OS_MAC)
    _ui->_treeWidget->setMinimumWidth(400);
#endif
}

IssuesWidget::~IssuesWidget()
{
    delete _ui;
}

void IssuesWidget::showEvent(QShowEvent *ev)
{
    ConfigFile cfg;
    cfg.restoreGeometryHeader(_ui->_treeWidget->header());
    QWidget::showEvent(ev);
}

void IssuesWidget::hideEvent(QHideEvent *ev)
{
    ConfigFile cfg;
    cfg.saveGeometryHeader(_ui->_treeWidget->header());
    QWidget::hideEvent(ev);
}

void IssuesWidget::cleanItems(const QString &folder)
{
    // The issue list is a state, clear it and let the next sync fill it
    // with ignored files and propagation errors.
    int itemCnt = _ui->_treeWidget->topLevelItemCount();
    for (int cnt = itemCnt - 1; cnt >= 0; cnt--) {
        QTreeWidgetItem *item = _ui->_treeWidget->topLevelItem(cnt);
        QString itemFolder = item->data(2, Qt::UserRole).toString();
        if (itemFolder == folder) {
            delete item;
        }
    }
    // update the tabtext
    emit(issueCountUpdated(_ui->_treeWidget->topLevelItemCount()));
}

void IssuesWidget::addItem(QTreeWidgetItem *item)
{
    if (!item)
        return;

    int insertLoc = 0;

    // Insert item specific errors behind the others
    if (!item->text(1).isEmpty()) {
        for (int i = 0; i < _ui->_treeWidget->topLevelItemCount(); ++i) {
            if (!_ui->_treeWidget->topLevelItem(i)->text(1).isEmpty()) {
                insertLoc = i;
                break;
            }
        }
    }

    _ui->_treeWidget->insertTopLevelItem(insertLoc, item);
    item->setHidden(!shouldBeVisible(item, currentAccountFilter(), currentFolderFilter()));
    emit issueCountUpdated(_ui->_treeWidget->topLevelItemCount());
}

void IssuesWidget::slotOpenFile(QTreeWidgetItem *item, int)
{
    QString folderName = item->data(2, Qt::UserRole).toString();
    QString fileName = item->text(1);

    Folder *folder = FolderMan::instance()->folder(folderName);
    if (folder) {
        // folder->path() always comes back with trailing path
        QString fullPath = folder->path() + fileName;
        if (QFile(fullPath).exists()) {
            showInFileManager(fullPath);
        }
    }
}

void IssuesWidget::slotProgressInfo(const QString &folder, const ProgressInfo &progress)
{
    if (progress.status() == ProgressInfo::Starting) {
        // The sync is restarting, clean the old items
        cleanItems(folder);
    }
}

void IssuesWidget::slotItemCompleted(const QString &folder, const SyncFileItemPtr &item)
{
    if (!item->hasErrorStatus())
        return;
    QTreeWidgetItem *line = ProtocolWidget::createCompletedTreewidgetItem(folder, *item);
    if (!line)
        return;
    addItem(line);
}

void IssuesWidget::slotRefreshIssues()
{
    auto tree = _ui->_treeWidget;
    auto filterFolderAlias = currentFolderFilter();
    auto filterAccount = currentAccountFilter();

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto item = tree->topLevelItem(i);
        item->setHidden(!shouldBeVisible(item, filterAccount, filterFolderAlias));
    }

    _ui->_treeWidget->setColumnHidden(2, !filterFolderAlias.isEmpty());
}

void IssuesWidget::slotAccountAdded(AccountState *account)
{
    _ui->filterAccount->addItem(account->account()->displayName(), QVariant::fromValue(account));
    updateAccountChoiceVisibility();
}

void IssuesWidget::slotAccountRemoved(AccountState *account)
{
    for (int i = _ui->filterAccount->count() - 1; i >= 0; --i) {
        if (account == _ui->filterAccount->itemData(i).value<AccountState *>())
            _ui->filterAccount->removeItem(i);
    }
    updateAccountChoiceVisibility();
}

void IssuesWidget::updateAccountChoiceVisibility()
{
    bool visible = _ui->filterAccount->count() > 2;
    _ui->filterAccount->setVisible(visible);
    _ui->accountLabel->setVisible(visible);
    slotUpdateFolderFilters();
}

AccountState *IssuesWidget::currentAccountFilter() const
{
    return _ui->filterAccount->currentData().value<AccountState *>();
}

QString IssuesWidget::currentFolderFilter() const
{
    return _ui->filterFolder->currentData().toString();
}

bool IssuesWidget::shouldBeVisible(QTreeWidgetItem *item, AccountState *filterAccount,
    const QString &filterFolderAlias) const
{
    bool visible = true;
    auto status = item->data(0, Qt::UserRole);
    visible &= (_ui->showIgnores->isChecked() || status != SyncFileItem::FileIgnored);
    visible &= (_ui->showWarnings->isChecked()
        || (status != SyncFileItem::SoftError
               && status != SyncFileItem::Conflict
               && status != SyncFileItem::Restoration));

    auto folderalias = item->data(2, Qt::UserRole).toString();
    if (filterAccount) {
        auto folder = FolderMan::instance()->folder(folderalias);
        visible &= folder && folder->accountState() == filterAccount;
    }
    visible &= (filterFolderAlias.isEmpty() || filterFolderAlias == folderalias);

    return visible;
}

void IssuesWidget::slotUpdateFolderFilters()
{
    auto account = _ui->filterAccount->currentData().value<AccountState *>();

    // If there is no account selector, show folders for the single
    // available account
    if (_ui->filterAccount->isHidden() && _ui->filterAccount->count() > 1) {
        account = _ui->filterAccount->itemData(1).value<AccountState *>();
    }

    if (!account) {
        _ui->filterFolder->setCurrentIndex(0);
    }
    _ui->filterFolder->setEnabled(account != 0);

    for (int i = _ui->filterFolder->count() - 1; i >= 1; --i) {
        _ui->filterFolder->removeItem(i);
    }

    // Find all selectable folders while figuring out if we need a folder
    // selector in the first place
    bool anyAccountHasMultipleFolders = false;
    QSet<AccountState *> accountsWithFolders;
    for (auto folder : FolderMan::instance()->map().values()) {
        if (accountsWithFolders.contains(folder->accountState()))
            anyAccountHasMultipleFolders = true;
        accountsWithFolders.insert(folder->accountState());

        if (folder->accountState() != account)
            continue;
        _ui->filterFolder->addItem(folder->shortGuiLocalPath(), folder->alias());
    }

    // If we don't need the combo box, hide it.
    _ui->filterFolder->setVisible(anyAccountHasMultipleFolders);
    _ui->folderLabel->setVisible(anyAccountHasMultipleFolders);

    // If there's no choice, select the only folder and disable
    if (_ui->filterFolder->count() == 2 && anyAccountHasMultipleFolders) {
        _ui->filterFolder->setCurrentIndex(1);
        _ui->filterFolder->setEnabled(false);
    }
}

void IssuesWidget::storeSyncIssues(QTextStream &ts)
{
    int topLevelItems = _ui->_treeWidget->topLevelItemCount();

    for (int i = 0; i < topLevelItems; i++) {
        QTreeWidgetItem *child = _ui->_treeWidget->topLevelItem(i);
        if (child->isHidden())
            continue;
        ts << right
           // time stamp
           << qSetFieldWidth(20)
           << child->data(0, Qt::DisplayRole).toString()
           // separator
           << qSetFieldWidth(0) << ","

           // file name
           << qSetFieldWidth(64)
           << child->data(1, Qt::DisplayRole).toString()
           // separator
           << qSetFieldWidth(0) << ","

           // folder
           << qSetFieldWidth(30)
           << child->data(2, Qt::DisplayRole).toString()
           // separator
           << qSetFieldWidth(0) << ","

           // action
           << qSetFieldWidth(15)
           << child->data(3, Qt::DisplayRole).toString()
           << qSetFieldWidth(0)
           << endl;
    }
}

void IssuesWidget::showFolderErrors(const QString &folderAlias)
{
    auto folder = FolderMan::instance()->folder(folderAlias);
    if (!folder)
        return;

    _ui->filterAccount->setCurrentIndex(
        qMax(0, _ui->filterAccount->findData(QVariant::fromValue(folder->accountState()))));
    _ui->filterFolder->setCurrentIndex(
        qMax(0, _ui->filterFolder->findData(folderAlias)));
    _ui->showIgnores->setChecked(false);
    _ui->showWarnings->setChecked(false);
}

void IssuesWidget::addError(const QString &folderAlias, const QString &message,
    ErrorCategory category)
{
    auto folder = FolderMan::instance()->folder(folderAlias);
    if (!folder)
        return;

    QStringList columns;
    QDateTime timestamp = QDateTime::currentDateTime();
    const QString timeStr = ProtocolWidget::timeString(timestamp);
    const QString longTimeStr = ProtocolWidget::timeString(timestamp, QLocale::LongFormat);

    columns << timeStr;
    columns << ""; // no "File" entry
    columns << folder->shortGuiLocalPath();
    columns << message;

    QIcon icon = Theme::instance()->syncStateIcon(SyncResult::Error);

    QTreeWidgetItem *twitem = new QTreeWidgetItem(columns);
    twitem->setData(0, Qt::SizeHintRole, QSize(0, ActivityItemDelegate::rowHeight()));
    twitem->setIcon(0, icon);
    twitem->setToolTip(0, longTimeStr);
    twitem->setToolTip(3, message);
    twitem->setData(0, Qt::UserRole, SyncFileItem::NormalError);
    twitem->setData(2, Qt::UserRole, folderAlias);

    addItem(twitem);
    addErrorWidget(twitem, message, category);
}

void IssuesWidget::addErrorWidget(QTreeWidgetItem *item, const QString &message, ErrorCategory category)
{
    QWidget *widget = 0;
    if (category == ErrorCategory::InsufficientRemoteStorage) {
        widget = new QWidget;
        auto layout = new QHBoxLayout;
        widget->setLayout(layout);

        auto label = new ElidedLabel(message, widget);
        label->setElideMode(Qt::ElideMiddle);
        layout->addWidget(label);

        auto button = new QPushButton("Retry all uploads", widget);
        button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);
        auto folderAlias = item->data(2, Qt::UserRole).toString();
        connect(button, &QPushButton::clicked,
            this, [this, folderAlias]() { retryInsufficentRemoteStorageErrors(folderAlias); });
        layout->addWidget(button);
    }

    if (widget) {
        item->setText(3, QString());
    }
    _ui->_treeWidget->setItemWidget(item, 3, widget);
}

void IssuesWidget::retryInsufficentRemoteStorageErrors(const QString &folderAlias)
{
    auto folderman = FolderMan::instance();
    auto folder = folderman->folder(folderAlias);
    if (!folder)
        return;

    folder->journalDb()->wipeErrorBlacklistCategory(SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage);
    folderman->scheduleFolderNext(folder);
}
}
