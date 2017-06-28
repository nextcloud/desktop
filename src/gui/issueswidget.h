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

#include "progressdispatcher.h"
#include "owncloudgui.h"

#include "ui_issueswidget.h"

class QPushButton;

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
    explicit IssuesWidget(QWidget *parent = 0);
    ~IssuesWidget();
    QSize sizeHint() const { return ownCloudGui::settingsDialogSize(); }

    void storeSyncIssues(QTextStream &ts);
    void showFolderErrors(const QString &folderAlias);

public slots:
    void addLine(const QString &folderAlias, const QString &message);
    void slotProgressInfo(const QString &folder, const ProgressInfo &progress);
    void slotItemCompleted(const QString &folder, const SyncFileItemPtr &item);
    void slotOpenFile(QTreeWidgetItem *item, int);

protected:
    void showEvent(QShowEvent *);
    void hideEvent(QHideEvent *);

signals:
    void copyToClipboard();
    void issueCountUpdated(int);

private slots:
    void slotRefreshIssues();
    void slotUpdateFolderFilters();
    void slotAccountAdded(AccountState *account);
    void slotAccountRemoved(AccountState *account);

private:
    void updateAccountChoiceVisibility();
    AccountState *currentAccountFilter() const;
    QString currentFolderFilter() const;
    bool shouldBeVisible(QTreeWidgetItem *item, AccountState *filterAccount,
        const QString &filterFolderAlias) const;
    void cleanItems(const QString &folder);
    void addItem(QTreeWidgetItem *item);

    Ui::IssuesWidget *_ui;
};
}

#endif
