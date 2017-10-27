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

#include "ui_protocolwidget.h"

class QPushButton;

namespace OCC {
class SyncResult;

namespace Ui {
    class ProtocolWidget;
}
class Application;

/**
 * A QTreeWidgetItem with special sorting.
 *
 * It allows items for global entries to be moved to the top if the
 * sorting section is the "Time" column.
 */
class SortedTreeWidgetItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;

private:
    bool operator<(const QTreeWidgetItem &other) const override;
};

/**
 * @brief The ProtocolWidget class
 * @ingroup gui
 */
class ProtocolWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ProtocolWidget(QWidget *parent = 0);
    ~ProtocolWidget();
    QSize sizeHint() const { return ownCloudGui::settingsDialogSize(); }

    void storeSyncActivity(QTextStream &ts);

    // Shared with IssueWidget
    static QTreeWidgetItem *createCompletedTreewidgetItem(const QString &folder, const SyncFileItem &item);
    static QString timeString(QDateTime dt, QLocale::FormatType format = QLocale::NarrowFormat);

public slots:
    void slotItemCompleted(const QString &folder, const SyncFileItemPtr &item);
    void slotOpenFile(QTreeWidgetItem *item, int);

protected:
    void showEvent(QShowEvent *);
    void hideEvent(QHideEvent *);

signals:
    void copyToClipboard();

private:
    Ui::ProtocolWidget *_ui;
};
}
#endif // PROTOCOLWIDGET_H
