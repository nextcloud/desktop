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

#include "account.h"
#include "accountmanager.h"
#include "commonstrings.h"
#include "folder.h"
#include "folderman.h"
#include "issueswidget.h"
#include "libsync/configfile.h"
#include "models/models.h"
#include "protocolwidget.h"
#include "syncengine.h"
#include "syncfileitem.h"
#include "theme.h"

#include "ui_issueswidget.h"

namespace {
bool persistsUntilLocalDiscovery(const OCC::ProtocolItem &data)
{
    return data.status() == OCC::SyncFileItem::Conflict
        || (data.status() == OCC::SyncFileItem::FileIgnored && data.direction() == OCC::SyncFileItem::Up)
        || data.status() == OCC::SyncFileItem::Excluded;
}

}
namespace OCC {

class SyncFileItemStatusSetSortFilterProxyModel : public Models::SignalledQSortFilterProxyModel
{
public:
    using StatusSet = std::array<bool, SyncFileItem::StatusCount>;

    explicit SyncFileItemStatusSetSortFilterProxyModel(QObject *parent = nullptr)
        : Models::SignalledQSortFilterProxyModel(parent)
    {
        restoreFilter();
    }

    ~SyncFileItemStatusSetSortFilterProxyModel() override
    {
    }

    StatusSet filter() const
    {
        return _filter;
    }

    void setFilter(const StatusSet &newFilter, bool save = true)
    {
        if (_filter != newFilter) {
            _filter = newFilter;
            if (save) {
                saveFilter();
            }
            invalidateFilter();
            Q_EMIT filterChanged();
        }
    }

    void resetFilter()
    {
        setFilter(defaultFilter());
    }

    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        QModelIndex idx = sourceModel()->index(sourceRow, filterKeyColumn(), sourceParent);

        bool ok = false;
        int sourceData = sourceModel()->data(idx, filterRole()).toInt(&ok);
        if (!ok) {
            return false;
        }

        return _filter[static_cast<SyncFileItem::Status>(sourceData)];
    }

    int filterCount()
    {
        StatusSet defaultSet = defaultFilter();
        OC_ASSERT(defaultSet.size() == _filter.size());

        int count = 0;
        // The number of filters is the number of items in the current filter that differ from the default set.
        for (size_t i = 0, ei = defaultSet.size(); i != ei; ++i) {
            // All errors are shown as a single filter, and they are all turned on or off together.
            // So to show them as 1 filter, ignore the first three errors...
            switch (i) {
            case SyncFileItem::Status::FatalError:
                Q_FALLTHROUGH();
            case SyncFileItem::Status::NormalError:
                Q_FALLTHROUGH();
            case SyncFileItem::Status::SoftError:
                break;

                // ... but *do* count the last one ...
            case SyncFileItem::Status::DetailError:
                Q_FALLTHROUGH();
            default:
                // ... just like the other status items:
                if (defaultSet[i] != _filter[i]) {
                    count += 1;
                }
            }
        }
        return count;
    }

private:
    static StatusSet defaultFilter()
    {
        StatusSet defaultSet;
        defaultSet.fill(true);
        defaultSet[SyncFileItem::NoStatus] = false;
        defaultSet[SyncFileItem::Success] = false;
        return defaultSet;
    }

    void saveFilter()
    {
        QStringList checked;
        for (uint8_t s = SyncFileItem::NoStatus; s < SyncFileItem::StatusCount; ++s) {
            if (_filter[s]) {
                checked.append(Utility::enumToString(static_cast<SyncFileItem::Status>(s)));
            }
        }
        ConfigFile().setIssuesWidgetFilter(checked);
    }

    void restoreFilter()
    {
        StatusSet filter = {};
        bool filterNeedsReset = true; // If there is no filter, the `true` value will cause a reset.
        std::optional<QStringList> checked = ConfigFile().issuesWidgetFilter();

        if (checked.has_value()) {
            // There is a filter, but it can be empty (user unchecked all checkboxes), and in that case we do not want to reset the filter.
            filterNeedsReset = false;

            for (const QString &s : checked.value()) {
                auto status = Utility::stringToEnum<SyncFileItem::Status>(s);
                if (static_cast<int8_t>(status) == -1) {
                    // The string value is not a valid enum value, so stop processing, and queue a reset.
                    filterNeedsReset = true;
                    break;
                } else {
                    filter[status] = true;
                }
            }
        }

        if (filterNeedsReset) {
            // If there was no filter in the config file, or if one of the values is invalid, reset the filter.
            resetFilter();
        } else {
            // There is a valid filter, so apply it. Also don't save it, because we just loaded it successfully
            setFilter(filter, false);
        }
    }

private:
    StatusSet _filter = {};
};

/**
 * If more issues are reported than this they will not show up
 * to avoid performance issues around sorting this many issues.
 */

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
        this, [this](Folder *folder, const QString &message, ErrorCategory) {
            auto item = SyncFileItemPtr::create();
            item->_status = SyncFileItem::NormalError;
            item->_errorString = message;
            _model->addProtocolItem(ProtocolItem { folder, item });
        });

    connect(ProgressDispatcher::instance(), &ProgressDispatcher::excluded, this, [this](Folder *f, const QString &file) {
        auto item = SyncFileItemPtr::create();
        item->_status = SyncFileItem::FilenameReserved;
        item->_file = file;
        item->_errorString = tr("The file %1 was ignored as its name is reserved by %2").arg(file, Theme::instance()->appNameGUI());
        _model->addProtocolItem(ProtocolItem { f, item });
    });

    _model = new ProtocolItemModel(20000, true, this);
    _sortModel = new Models::SignalledQSortFilterProxyModel(this);
    connect(_sortModel, &Models::SignalledQSortFilterProxyModel::filterChanged, this, &IssuesWidget::filterDidChange);
    _sortModel->setSourceModel(_model);
    _statusSortModel = new SyncFileItemStatusSetSortFilterProxyModel(this); // Note: this will restore a previously set filter, if there was one.
    connect(_statusSortModel, &Models::SignalledQSortFilterProxyModel::filterChanged, this, &IssuesWidget::filterDidChange);
    _statusSortModel->setSourceModel(_sortModel);
    _statusSortModel->setSortRole(Qt::DisplayRole); // Sorting should be done based on the text in the column cells, but...
    _statusSortModel->setFilterRole(Models::UnderlyingDataRole); // ... filtering should be done on the underlying enum value.
    _statusSortModel->setFilterKeyColumn(static_cast<int>(ProtocolItemModel::ProtocolItemRole::Status));
    _ui->_tableView->setModel(_statusSortModel);

    auto header = new ExpandingHeaderView(QStringLiteral("ActivityErrorListHeaderV2"), _ui->_tableView);
    _ui->_tableView->setHorizontalHeader(header);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setExpandingColumn(static_cast<int>(ProtocolItemModel::ProtocolItemRole::Action));
    header->setSortIndicator(static_cast<int>(ProtocolItemModel::ProtocolItemRole::Time), Qt::DescendingOrder);

    connect(_ui->_tableView, &QTreeView::customContextMenuRequested, this, &IssuesWidget::slotItemContextMenu);
    _ui->_tableView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, &QHeaderView::customContextMenuRequested, [this, header]() {
        auto menu = showFilterMenu(header);
        menu->addAction(tr("Reset column sizes"), header, [header] { header->resizeColumns(true); });
    });

    connect(_ui->_filterButton, &QAbstractButton::clicked, this, [this] {
        showFilterMenu(_ui->_filterButton);
    });
    filterDidChange(); // Set the appropriate label.

    _ui->_tooManyIssuesWarning->hide();
    connect(_model, &ProtocolItemModel::rowsInserted, this, [this] {
        Q_EMIT issueCountUpdated(_model->rowCount());
        _ui->_tooManyIssuesWarning->setVisible(_model->isModelFull());
    });
    connect(_model, &ProtocolItemModel::modelReset, this, [this] {
        Q_EMIT issueCountUpdated(_model->rowCount());
        _ui->_tooManyIssuesWarning->setVisible(_model->isModelFull());
    });

    _ui->_conflictHelp->hide();
    _ui->_conflictHelp->setText(
        tr("There were conflicts. <a href=\"%1\">Check the documentation on how to resolve them.</a>")
            .arg(Theme::instance()->conflictHelpUrl()));

    connect(FolderMan::instance(), &FolderMan::folderRemoved, this, [this](Folder *f) {
        _model->remove_if([f](const ProtocolItem &item) {
            return item.folder() == f;
        });
    });
}

IssuesWidget::~IssuesWidget()
{
    delete _ui;
}

QMenu *IssuesWidget::showFilterMenu(QWidget *parent)
{
    auto menu = new QMenu(parent);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto accountFilterReset = Models::addFilterMenuItems(menu, AccountManager::instance()->accountNames(), _sortModel, static_cast<int>(ProtocolItemModel::ProtocolItemRole::Account), tr("Account"), Qt::DisplayRole);
    menu->addSeparator();
    auto statusFilterReset = addStatusFilter(menu);
    menu->addSeparator();
    addResetFiltersAction(menu, { accountFilterReset, statusFilterReset });

    QTimer::singleShot(0, menu, [menu] {
        menu->popup(QCursor::pos());
    });

    return menu;
}

void IssuesWidget::addResetFiltersAction(QMenu *menu, const QList<std::function<void()>> &resetFunctions)
{
    menu->addAction(QCoreApplication::translate("OCC::Models", "Reset Filters"), [resetFunctions]() {
        for (const auto &reset : resetFunctions) {
            reset();
        }
    });
}

void IssuesWidget::slotProgressInfo(Folder *folder, const ProgressInfo &progress)
{
    if (progress.status() == ProgressInfo::Reconcile) {
        // Wipe all non-persistent entries - as well as the persistent ones
        // in cases where a local discovery was done.
        const auto &engine = folder->syncEngine();
        const auto style = engine.lastLocalDiscoveryStyle();
        _model->remove_if([&](const ProtocolItem &item) {
            if (item.folder() != folder) {
                return false;
            }
            if (item.direction() == SyncFileItem::None && item.status() == SyncFileItem::FilenameReserved) {
                // TODO: don't clear syncErrors and excludes for now.
                // make them either unique or remove them on the next sync?
                return false;
            }
            if (style == LocalDiscoveryStyle::FilesystemOnly) {
                return true;
            }
            if (!persistsUntilLocalDiscovery(item)) {
                return true;
            }
            // Definitely wipe the entry if the file no longer exists
            if (!QFileInfo::exists(folder->path() + item.path())) {
                return true;
            }

            auto path = QFileInfo(item.path()).dir().path();
            if (path == QLatin1Char('.'))
                path.clear();

            return engine.shouldDiscoverLocally(path);
        });
    }
    if (progress.status() == ProgressInfo::Done) {
        // We keep track very well of pending conflicts.
        // Inform other components about them.
        QStringList conflicts;
        for (const auto &rawData : _model->rawData()) {
            if (rawData.folder() == folder && rawData.status() == SyncFileItem::Conflict) {
                conflicts.append(rawData.path());
            }
        }
        Q_EMIT ProgressDispatcher::instance()->folderConflicts(folder, conflicts);

        _ui->_conflictHelp->setHidden(Theme::instance()->conflictHelpUrl().isEmpty() || conflicts.isEmpty());
    }
}

void IssuesWidget::slotItemCompleted(Folder *folder, const SyncFileItemPtr &item)
{
    if (!item->showInIssuesTab())
        return;
    _model->addProtocolItem(ProtocolItem { folder, item });
}

void IssuesWidget::filterDidChange()
{
    // We have two filters here: the filter by status (which can have multiple items checked *off*...
    int filterCount = _statusSortModel->filterCount();
    // .. and the filter on the account name, which can only be 1 item checked:
    if (!_sortModel->filterRegularExpression().pattern().isEmpty()) {
        filterCount += 1;
    }

    _ui->_filterButton->setText(filterCount > 0 ? CommonStrings::filterButtonText(filterCount) : tr("Filter"));
}

void IssuesWidget::slotItemContextMenu()
{
    auto rows = _ui->_tableView->selectionModel()->selectedRows();
    for (int i = 0; i < rows.size(); ++i) {
        rows[i] = _statusSortModel->mapToSource(rows[i]);
        rows[i] = _sortModel->mapToSource(rows[i]);
    }
    ProtocolWidget::showContextMenu(this, _model, rows);
}

std::function<void(void)> IssuesWidget::addStatusFilter(QMenu *menu)
{
    menu->addAction(QCoreApplication::translate("OCC::Models", "Status Filter:"))->setEnabled(false);

    // Use a QActionGroup to contain all status filter items, so we can find them back easily to reset.
    auto statusFilterGroup = new QActionGroup(menu);
    statusFilterGroup->setExclusive(false);

    const auto initialFilter = _statusSortModel->filter();

    { // Add all errors under 1 action:
        auto action = menu->addAction(Utility::enumToDisplayName(SyncFileItem::NormalError), this, [this](bool checked) {
            auto currentFilter = _statusSortModel->filter();
            for (const auto &item : SyncFileItem::ErrorStatusItems) {
                currentFilter[item] = checked;
            }
            _statusSortModel->setFilter(currentFilter);
        });
        action->setCheckable(true);
        action->setChecked(initialFilter[SyncFileItem::ErrorStatusItems[0]]);
        statusFilterGroup->addAction(action);
    }
    menu->addSeparator();

    // list of OtherDisplayableStatusItems with the localised name
    std::vector<std::pair<QString, SyncFileItem::Status>> otherStatusItems;
    otherStatusItems.reserve(SyncFileItem::OtherDisplayableStatusItems.size());
    for (const auto &item : SyncFileItem::OtherDisplayableStatusItems) {
        otherStatusItems.emplace_back(Utility::enumToDisplayName(item), item);
    }
    std::sort(otherStatusItems.begin(), otherStatusItems.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
    });
    for (const auto &item : otherStatusItems) {
        auto action = menu->addAction(item.first, menu, [this, item](bool checked) {
            auto currentFilter = _statusSortModel->filter();
            currentFilter[item.second] = checked;
            _statusSortModel->setFilter(currentFilter);
        });
        action->setCheckable(true);
        action->setChecked(initialFilter[item.second]);
        statusFilterGroup->addAction(action);
    }

    menu->addSeparator();

    // Add action to reset all filters at once:
    return [statusFilterGroup, this]() {
        for (QAction *action : statusFilterGroup->actions()) {
            action->setChecked(true);
        }
        _statusSortModel->resetFilter();
    };
}
}
