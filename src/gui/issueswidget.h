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

#ifndef ISSUESWIDGET_H
#define ISSUESWIDGET_H

#include <QDialog>
#include <QDateTime>
#include <QLocale>
#include <QTimer>

#include "models/expandingheaderview.h"
#include "models/models.h"
#include "models/protocolitemmodel.h"
#include "owncloudgui.h"
#include "progressdispatcher.h"

#include "ui_issueswidget.h"

class QSortFilterProxyModel;

namespace OCC {
class SyncResult;
class SyncFileItemStatusSetSortFilterProxyModel;

namespace Ui {
    class ProtocolWidget;
}
class Application;

/**
 * @brief The ProtocolWidget class
 * @ingroup gui
 */
class IssuesWidget : public QWidget
{
    Q_OBJECT
public:
    explicit IssuesWidget(QWidget *parent = nullptr);
    ~IssuesWidget() override;

public slots:
    void slotProgressInfo(Folder *folder, const ProgressInfo &progress);
    void slotItemCompleted(Folder *folder, const SyncFileItemPtr &item);
    void filterDidChange();

signals:
    void issueCountUpdated(int);

private slots:
    QMenu *showFilterMenu(QWidget *parent);
    void slotItemContextMenu();

private:
    static void addResetFiltersAction(QMenu *menu, const QList<std::function<void()>> &resetFunctions);
    std::function<void()> addStatusFilter(QMenu *menu);

    ProtocolItemModel *_model;
    SignalledQSortFilterProxyModel *_sortModel;
    SyncFileItemStatusSetSortFilterProxyModel *_statusSortModel;

    Ui::IssuesWidget *_ui;
};
}

#endif
