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

#include "protocolitemmodel.h"
#include "progressdispatcher.h"
#include "owncloudgui.h"

#include "ui_issueswidget.h"

class QSortFilterProxyModel;

namespace OCC {
class SyncResult;

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
    //    void addError(const QString &folderAlias, const QString &message, ErrorCategory category);
    void slotProgressInfo(const QString &folder, const ProgressInfo &progress);
    void slotItemCompleted(const QString &folder, const SyncFileItemPtr &item);

signals:
    void issueCountUpdated(int);

private slots:
    void slotItemContextMenu();

private:
    /// Wipes all insufficient remote storgage blacklist entries
    //    void retryInsufficentRemoteStorageErrors(const QString &folderAlias);

    ProtocolItemModel *_model;
    QSortFilterProxyModel *_sortModel;

    Ui::IssuesWidget *_ui;
};
}

#endif
