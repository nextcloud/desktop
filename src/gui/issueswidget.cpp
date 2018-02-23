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
#include <QtWidgets>

#include "issueswidget.h"
#include "configfile.h"
#include "syncresult.h"
#include "syncengine.h"
#include "logger.h"
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
#include "common/syncjournalfilerecord.h"
#include "elidedlabel.h"


#include "ui_issueswidget.h"

#include <climits>

namespace OCC {

/**
 * If more issues are reported than this they will not show up
 * to avoid performance issues around sorting this many issues.
 */
static const int maxIssueCount = 50000;

IssuesWidget::IssuesWidget(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::IssuesWidget)
{
    _ui->setupUi(this);

    connect(ProgressDispatcher::instance(), &ProgressDispatcher::progressInfo,
        this, &IssuesWidget::slotProgressInfo);
    connect(ProgressDispatcher::instance(), &ProgressDispatcher::itemCompleted,
        this, &IssuesWidget::slotItemCompleted);
    connect(ProgressDispatcher::instance(), &ProgressDispatcher::syncError,
        this, &IssuesWidget::addError);

    connect(_ui->_treeWidget, &QTreeWidget::itemActivated, this, &IssuesWidget::slotOpenFile);
    connect(_ui->copyIssuesButton, &QAbstractButton::clicked, this, &IssuesWidget::copyToClipboard);

    _ui->_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(_ui->_treeWidget, &QTreeWidget::customContextMenuRequested, this, &IssuesWidget::slotItemContextMenu);

    connect(_ui->showIgnores, &QAbstractButton::toggled, this, &IssuesWidget::slotRefreshIssues);
    connect(_ui->showWarnings, &QAbstractButton::toggled, this, &IssuesWidget::slotRefreshIssues);
    connect(_ui->filterAccount, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &IssuesWidget::slotRefreshIssues);
    connect(_ui->filterAccount, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &IssuesWidget::slotUpdateFolderFilters);
    connect(_ui->filterFolder, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &IssuesWidget::slotRefreshIssues);
    for (auto account : AccountManager::instance()->accounts()) {
        slotAccountAdded(account.data());
    }
    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, &IssuesWidget::slotAccountAdded);
    connect(AccountManager::instance(), &AccountManager::accountRemoved,
        this, &IssuesWidget::slotAccountRemoved);
    connect(FolderMan::instance(), &FolderMan::folderListChanged,
        this, &IssuesWidget::slotUpdateFolderFilters);


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
        + _ui->_treeWidget->fontMetrics().width(ProtocolItem::timeString(QDateTime::currentDateTime()))
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

    _reenableSorting.setInterval(5000);
    connect(&_reenableSorting, &QTimer::timeout, this,
        [this]() { _ui->_treeWidget->setSortingEnabled(true); });

    _ui->_tooManyIssuesWarning->hide();
    connect(this, &IssuesWidget::issueCountUpdated, this,
        [this](int count) { _ui->_tooManyIssuesWarning->setVisible(count >= maxIssueCount); });
}

IssuesWidget::~IssuesWidget()
{
    delete _ui;
}

void IssuesWidget::showEvent(QShowEvent *ev)
{
    ConfigFile cfg;
    cfg.restoreGeometryHeader(_ui->_treeWidget->header());

    // Sorting by section was newly enabled. But if we restore the header
    // from a state where sorting was disabled, both of these flags will be
    // false and sorting will be impossible!
    _ui->_treeWidget->header()->setSectionsClickable(true);
    _ui->_treeWidget->header()->setSortIndicatorShown(true);

    // Switch back to "first important, then by time" ordering
    _ui->_treeWidget->sortByColumn(0, Qt::DescendingOrder);

    QWidget::showEvent(ev);
}

void IssuesWidget::hideEvent(QHideEvent *ev)
{
    ConfigFile cfg;
    cfg.saveGeometryHeader(_ui->_treeWidget->header());
    QWidget::hideEvent(ev);
}

static bool persistsUntilLocalDiscovery(QTreeWidgetItem *item)
{
    const auto data = ProtocolItem::extraData(item);
    return data.status == SyncFileItem::Conflict
        || (data.status == SyncFileItem::FileIgnored && data.direction == SyncFileItem::Up);
}

void IssuesWidget::cleanItems(const std::function<bool(QTreeWidgetItem *)> &shouldDelete)
{
    _ui->_treeWidget->setSortingEnabled(false);

    // The issue list is a state, clear it and let the next sync fill it
    // with ignored files and propagation errors.
    int itemCnt = _ui->_treeWidget->topLevelItemCount();
    for (int cnt = itemCnt - 1; cnt >= 0; cnt--) {
        QTreeWidgetItem *item = _ui->_treeWidget->topLevelItem(cnt);
        if (shouldDelete(item))
            delete item;
    }

    _ui->_treeWidget->setSortingEnabled(true);

    // update the tabtext
    emit(issueCountUpdated(_ui->_treeWidget->topLevelItemCount()));
}

void IssuesWidget::addItem(QTreeWidgetItem *item)
{
    if (!item)
        return;

    int count = _ui->_treeWidget->topLevelItemCount();
    if (count >= maxIssueCount)
        return;

    _ui->_treeWidget->setSortingEnabled(false);
    _reenableSorting.start();

    // Insert item specific errors behind the others
    int insertLoc = 0;
    if (!item->text(1).isEmpty()) {
        for (int i = 0; i < count; ++i) {
            if (_ui->_treeWidget->topLevelItem(i)->text(1).isEmpty()) {
                insertLoc = i + 1;
            } else {
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
    QString fileName = item->text(1);
    if (Folder *folder = ProtocolItem::folder(item)) {
        // folder->path() always comes back with trailing path
        QString fullPath = folder->path() + fileName;
        if (QFile(fullPath).exists()) {
            showInFileManager(fullPath);
        }
    }
}

void IssuesWidget::slotProgressInfo(const QString &folder, const ProgressInfo &progress)
{
    if (progress.status() == ProgressInfo::Reconcile) {
        // Wipe all non-persistent entries - as well as the persistent ones
        // in cases where a local discovery was done.
        auto f = FolderMan::instance()->folder(folder);
        if (!f)
            return;
        const auto &engine = f->syncEngine();
        const auto style = engine.lastLocalDiscoveryStyle();
        cleanItems([&](QTreeWidgetItem *item) {
            if (ProtocolItem::extraData(item).folderName != folder)
                return false;
            if (style == LocalDiscoveryStyle::FilesystemOnly)
                return true;
            if (!persistsUntilLocalDiscovery(item))
                return true;

            // Definitely wipe the entry if the file no longer exists
            if (!QFileInfo(f->path() + ProtocolItem::extraData(item).path).exists())
                return true;

            auto path = QFileInfo(ProtocolItem::extraData(item).path).dir().path().toUtf8();
            if (path == ".")
                path.clear();

            return engine.shouldDiscoverLocally(path);
        });
    }
    if (progress.status() == ProgressInfo::Done) {
        // We keep track very well of pending conflicts.
        // Inform other components about them.
        QStringList conflicts;
        auto tree = _ui->_treeWidget;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            auto item = tree->topLevelItem(i);
            auto data = ProtocolItem::extraData(item);
            if (data.folderName == folder
                && data.status == SyncFileItem::Conflict) {
                conflicts.append(data.path);
            }
        }
        emit ProgressDispatcher::instance()->folderConflicts(folder, conflicts);
    }
}

void IssuesWidget::slotItemCompleted(const QString &folder, const SyncFileItemPtr &item)
{
    if (!item->showInIssuesTab())
        return;
    QTreeWidgetItem *line = ProtocolItem::create(folder, *item);
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

void IssuesWidget::slotItemContextMenu(const QPoint &pos)
{
    auto item = _ui->_treeWidget->itemAt(pos);
    if (!item)
        return;
    auto globalPos = _ui->_treeWidget->viewport()->mapToGlobal(pos);
    ProtocolItem::openContextMenu(globalPos, item, this);
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
    auto data = ProtocolItem::extraData(item);
    auto status = data.status;
    visible &= (_ui->showIgnores->isChecked() || status != SyncFileItem::FileIgnored);
    visible &= (_ui->showWarnings->isChecked()
        || (status != SyncFileItem::SoftError
               && status != SyncFileItem::Restoration));

    const auto &folderalias = data.folderName;
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
    const QString timeStr = ProtocolItem::timeString(timestamp);
    const QString longTimeStr = ProtocolItem::timeString(timestamp, QLocale::LongFormat);

    columns << timeStr;
    columns << ""; // no "File" entry
    columns << folder->shortGuiLocalPath();
    columns << message;

    QIcon icon = Theme::instance()->syncStateIcon(SyncResult::Error);

    QTreeWidgetItem *twitem = new ProtocolItem(columns);
    twitem->setData(0, Qt::SizeHintRole, QSize(0, ActivityItemDelegate::rowHeight()));
    twitem->setIcon(0, icon);
    twitem->setToolTip(0, longTimeStr);
    twitem->setToolTip(3, message);
    ProtocolItem::ExtraData data;
    data.timestamp = timestamp;
    data.folderName = folderAlias;
    data.status = SyncFileItem::NormalError;
    ProtocolItem::setExtraData(twitem, data);

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
        auto folderAlias = ProtocolItem::extraData(item).folderName;
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
