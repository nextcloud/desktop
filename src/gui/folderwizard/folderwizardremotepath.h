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
#pragma once


#include "gui/folder.h"
#include "gui/folderwizard/folderwizard_p.h"

#include <QWizardPage>


class QTreeWidgetItem;

class Ui_FolderWizardTargetPage;

namespace OCC {

/**
 * @brief page to ask for the target folder
 * @ingroup gui
 */

class FolderWizardRemotePath : public FolderWizardPage
{
    Q_OBJECT
public:
    explicit FolderWizardRemotePath(FolderWizardPrivate *parent);
    ~FolderWizardRemotePath() override;

    bool isComplete() const override;

    void initializePage() override;
    void cleanupPage() override;

    const QString &targetPath() const;

protected Q_SLOTS:

    void showWarn(const QString & = QString()) const;
    void slotAddRemoteFolder();
    void slotCreateRemoteFolder(const QString &);
    void slotCreateRemoteFolderFinished();
    void slotHandleMkdirNetworkError(QNetworkReply *);
    void slotHandleLsColNetworkError();
    void slotUpdateDirectories(const QStringList &);
    void slotRefreshFolders();
    void slotItemExpanded(QTreeWidgetItem *);
    void slotCurrentItemChanged(QTreeWidgetItem *);
    void slotFolderEntryEdited(const QString &text);
    void slotLsColFolderEntry();
    void slotTypedPathFound(const QStringList &subpaths);

private:
    PropfindJob *runPropFindJob(const QString &path);
    void recursiveInsert(QTreeWidgetItem *parent, QStringList pathTrail, const QString &path);
    bool selectByPath(QString path);
    Ui_FolderWizardTargetPage *_ui;
    bool _warnWasVisible;
    QTimer _lscolTimer;

    QString _targetPath;
};

}
