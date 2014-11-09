/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#ifndef MIRALL_FOLDERWIZARD_H
#define MIRALL_FOLDERWIZARD_H

#include <QWizard>
#include <QNetworkReply>
#include <QTimer>

#include "folder.h"

#include "ui_folderwizardsourcepage.h"
#include "ui_folderwizardtargetpage.h"

namespace OCC {

class SelectiveSyncTreeView;

class ownCloudInfo;

class FormatWarningsWizardPage : public QWizardPage {
protected:
    QString formatWarnings(const QStringList &warnings) const;
};

/**
 * page to ask for the local source folder
 */
class FolderWizardLocalPath : public FormatWarningsWizardPage
{
    Q_OBJECT
public:
    FolderWizardLocalPath();
    ~FolderWizardLocalPath();

    virtual bool isComplete() const Q_DECL_OVERRIDE;
    void initializePage() Q_DECL_OVERRIDE;
    void cleanupPage() Q_DECL_OVERRIDE;

    void setFolderMap( const Folder::Map &fm ) { _folderMap = fm; }
protected slots:
    void slotChooseLocalFolder();

private:
    Ui_FolderWizardSourcePage _ui;
    Folder::Map _folderMap;
};


/**
 * page to ask for the target folder
 */

class FolderWizardRemotePath : public FormatWarningsWizardPage
{
    Q_OBJECT
public:
    FolderWizardRemotePath();
    ~FolderWizardRemotePath();

    virtual bool isComplete() const Q_DECL_OVERRIDE;

    virtual void initializePage() Q_DECL_OVERRIDE;
    virtual void cleanupPage() Q_DECL_OVERRIDE;

protected slots:

    void showWarn( const QString& = QString() ) const;
    void slotAddRemoteFolder();
    void slotCreateRemoteFolder(const QString&);
    void slotCreateRemoteFolderFinished(QNetworkReply::NetworkError error);
    void slotHandleNetworkError(QNetworkReply*);
    void slotUpdateDirectories(const QStringList&);
    void slotRefreshFolders();
    void slotItemExpanded(QTreeWidgetItem*);
private:
    void recursiveInsert(QTreeWidgetItem *parent, QStringList pathTrail, QString path);
    Ui_FolderWizardTargetPage _ui;
    bool _warnWasVisible;

};


class FolderWizardSelectiveSync : public QWizardPage
{
    Q_OBJECT
public:
    FolderWizardSelectiveSync();
    ~FolderWizardSelectiveSync();

    virtual bool validatePage() Q_DECL_OVERRIDE;

    virtual void initializePage() Q_DECL_OVERRIDE;
    virtual void cleanupPage() Q_DECL_OVERRIDE;

private:
    SelectiveSyncTreeView *_treeView;

};


/**
 *
 */
class FolderWizard : public QWizard
{
    Q_OBJECT
public:

    enum {
        Page_Source,
        Page_Target,
        Page_SelectiveSync
    };

    FolderWizard(QWidget *parent = 0);
    ~FolderWizard();

private:

    FolderWizardLocalPath *_folderWizardSourcePage;
    FolderWizardRemotePath *_folderWizardTargetPage;
    FolderWizardSelectiveSync *_folderWizardSelectiveSyncPage;
};


} // namespace OCC

#endif
