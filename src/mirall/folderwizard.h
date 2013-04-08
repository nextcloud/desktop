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
#include "ui_folderwizardnetworkpage.h"
#include "ui_folderwizardowncloudpage.h"


namespace Mirall {

class ownCloudInfo;
class ownCloudDirCheck;

/**
 * page to ask for the local source folder
 */
class FolderWizardSourcePage : public QWizardPage
{
    Q_OBJECT
public:
    FolderWizardSourcePage();
    ~FolderWizardSourcePage();

    virtual bool isComplete() const;
    void initializePage();
    void cleanupPage();

    void setFolderMap( Folder::Map *fm ) { _folderMap = fm; }
protected slots:
    void on_localFolderChooseBtn_clicked();
    void on_localFolderLineEdit_textChanged();

private:
    Ui_FolderWizardSourcePage _ui;
    Folder::Map *_folderMap;
};


/**
 * page to ask for the target folder
 */

class FolderWizardTargetPage : public QWizardPage
{
    Q_OBJECT
public:
    FolderWizardTargetPage();
    ~FolderWizardTargetPage();

    virtual bool isComplete() const;

    virtual void initializePage();
    virtual void cleanupPage();

protected slots:
    void slotToggleItems();
    void on_localFolder2ChooseBtn_clicked();

    void on_localFolderRadioBtn_toggled();
    void on_urlFolderRadioBtn_toggled();

    void on_localFolder2LineEdit_textChanged();
    void on_urlFolderLineEdit_textChanged();

    void slotOwnCloudFound( const QString&, const QString&, const QString&, const QString& );
    void slotNoOwnCloudFound(QNetworkReply*);

    void slotFolderTextChanged( const QString& );
    void slotTimerFires();
    void slotDirCheckReply( const QString&, QNetworkReply* );
    void showWarn( const QString& = QString(), bool showCreateButton = false ) const;
    void slotCreateRemoteFolder();
    void slotCreateRemoteFolderFinished( QNetworkReply::NetworkError error );

private:
    Ui_FolderWizardTargetPage _ui;
    QTimer *_timer;
    ownCloudInfo *_ownCloudDirCheck;
    bool _dirChecked;
    bool _warnWasVisible;
};

class FolderWizardNetworkPage : public QWizardPage
{
    Q_OBJECT
public:
    FolderWizardNetworkPage();
    ~FolderWizardNetworkPage();

    virtual bool isComplete() const;

protected slots:

private:
    Ui_FolderWizardNetworkPage _ui;
};

class FolderWizardOwncloudPage : public QWizardPage
{
    Q_OBJECT
public:
    FolderWizardOwncloudPage();
    ~FolderWizardOwncloudPage();

    virtual bool isComplete() const;
    void initializePage();

protected slots:

private:
    Ui_FolderWizardOwncloudPage _ui;

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
        Page_Network,
        Page_Owncloud
    };

    FolderWizard(QWidget *parent = 0);
    ~FolderWizard();
    void setFolderMap( Folder::Map* );

private:

    FolderWizardSourcePage *_folderWizardSourcePage;
    FolderWizardTargetPage *_folderWizardTargetPage;
};


} // ns Mirall

#endif
