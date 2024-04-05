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

#ifndef PROTOCOLWIDGET_H
#define PROTOCOLWIDGET_H

#include <QDialog>
#include <QDateTime>
#include <QLocale>

#include "progressdispatcher.h"
#include "owncloudgui.h"
#include "models/protocolitemmodel.h"

#include "protocolitem.h"

#include "models/models.h"

class QPushButton;
class QSortFilterProxyModel;

namespace OCC {
class ExpandingHeaderView;

namespace Ui {
    class ProtocolWidget;
}
class Application;

/**
 * @brief The ProtocolWidget class
 * @ingroup gui
 */
class ProtocolWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ProtocolWidget(QWidget *parent = nullptr);
    ~ProtocolWidget() override;

    static void showContextMenu(QWidget *parent, ProtocolItemModel *model, const QModelIndexList &items);
    static QMenu *showFilterMenu(QWidget *parent, Models::SignalledQSortFilterProxyModel *model, int role, const QString &columnName);

public Q_SLOTS:
    void slotItemCompleted(Folder *folder, const SyncFileItemPtr &item);
    void filterDidChange();

private Q_SLOTS:
    void slotItemContextMenu();

private:
    ProtocolItemModel *_model;
    Models::SignalledQSortFilterProxyModel *_sortModel;
    Ui::ProtocolWidget *_ui;
};
}
#endif // PROTOCOLWIDGET_H
