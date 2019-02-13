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
 * The items used in the protocol and issue QTreeWidget
 *
 * Special sorting: It allows items for global entries to be moved to the top if the
 * sorting section is the "Time" column.
 */
class ProtocolItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    // Shared with IssueWidget
    static ProtocolItem *create(const QString &folder, const SyncFileItem &item);
    static QString timeString(QDateTime dt, QLocale::FormatType format = QLocale::NarrowFormat);

    struct ExtraData
    {
        ExtraData()
            : status(SyncFileItem::NoStatus)
            , direction(SyncFileItem::None)
        {
        }

        QString path;
        QString folderName;
        QDateTime timestamp;
        qint64 size = 0;
        SyncFileItem::Status status BITFIELD(4);
        SyncFileItem::Direction direction BITFIELD(3);
    };

    static ExtraData extraData(const QTreeWidgetItem *item);
    static void setExtraData(QTreeWidgetItem *item, const ExtraData &data);

    static SyncJournalFileRecord syncJournalRecord(QTreeWidgetItem *item);
    static Folder *folder(QTreeWidgetItem *item);

    static void openContextMenu(QPoint globalPos, QTreeWidgetItem *item, QWidget *parent);

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

public slots:
    void slotItemCompleted(const QString &folder, const SyncFileItemPtr &item);
    void slotOpenFile(QTreeWidgetItem *item, int);

protected:
    void showEvent(QShowEvent *);
    void hideEvent(QHideEvent *);

private slots:
    void slotItemContextMenu(const QPoint &pos);

signals:
    void copyToClipboard();

private:
    Ui::ProtocolWidget *_ui;
};
}
#endif // PROTOCOLWIDGET_H
