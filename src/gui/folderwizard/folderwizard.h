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

#include <QNetworkReply>
#include <QTimer>
#include <QWizard>

#include "accountfwd.h"

#include "gui/folder.h"

class QCheckBox;
class QTreeWidgetItem;

class Ui_FolderWizardSourcePage;
class Ui_FolderWizardTargetPage;

namespace OCC {

class SelectiveSyncWidget;

class ownCloudInfo;

/**
 * @brief The FormatWarningsWizardPage class
 * @ingroup gui
 */
class FormatWarningsWizardPage : public QWizardPage
{
    Q_OBJECT
protected:
    QString formatWarnings(const QStringList &warnings, bool isError = false) const;
};

/**
 * @brief Page to ask for the local source folder
 * @ingroup gui
 */
class FolderWizardLocalPath : public QWizardPage
{
    Q_OBJECT
public:
    explicit FolderWizardLocalPath(const AccountPtr &account, QWidget *parent = nullptr);
    ~FolderWizardLocalPath() override;

    bool isComplete() const override;
    void initializePage() override;
    void cleanupPage() override;
protected slots:
    void slotChooseLocalFolder();

private:
    Ui_FolderWizardSourcePage *_ui;
    QMap<QString, Folder *> _folderMap;
    AccountPtr _account;
};


/**
 * @brief page to ask for the target folder
 * @ingroup gui
 */

class FolderWizardRemotePath : public QWizardPage
{
    Q_OBJECT
public:
    explicit FolderWizardRemotePath(const AccountPtr &account, QWidget *parent = nullptr);
    ~FolderWizardRemotePath() override;

    bool isComplete() const override;

    void initializePage() override;
    void cleanupPage() override;

protected slots:

    void showWarn(const QString & = QString()) const;
    void slotAddRemoteFolder();
    void slotCreateRemoteFolder(const QString &);
    void slotCreateRemoteFolderFinished();
    void slotHandleMkdirNetworkError(QNetworkReply *);
    void slotHandleLsColNetworkError(QNetworkReply *);
    void slotUpdateDirectories(const QStringList &);
    void slotRefreshFolders();
    void slotItemExpanded(QTreeWidgetItem *);
    void slotCurrentItemChanged(QTreeWidgetItem *);
    void slotFolderEntryEdited(const QString &text);
    void slotLsColFolderEntry();
    void slotTypedPathFound(const QStringList &subpaths);

private:
    LsColJob *runLsColJob(const QString &path);
    void recursiveInsert(QTreeWidgetItem *parent, QStringList pathTrail, QString path);
    bool selectByPath(QString path);
    Ui_FolderWizardTargetPage *_ui;
    bool _warnWasVisible;
    AccountPtr _account;
    QTimer _lscolTimer;
};

/**
 * @brief The FolderWizardSelectiveSync class
 * @ingroup gui
 */
class FolderWizardSelectiveSync : public QWizardPage
{
    Q_OBJECT
public:
    explicit FolderWizardSelectiveSync(const AccountPtr &account, QWidget *parent = nullptr);
    ~FolderWizardSelectiveSync() override;

    bool validatePage() override;

    void initializePage() override;
    void cleanupPage() override;

private slots:
    void virtualFilesCheckboxClicked();

private:
    SelectiveSyncWidget *_selectiveSync;
    QCheckBox *_virtualFilesCheckBox = nullptr;
};

/**
 * @brief The FolderWizard class
 * @ingroup gui
 */
class FolderWizard : public QWizard
{
    Q_OBJECT
public:
    enum {
        Page_Space,
        Page_Source,
        Page_Target,
        Page_SelectiveSync
    };

    explicit FolderWizard(AccountPtr account, QWidget *parent = nullptr, Qt::WindowFlags flags = {});
    ~FolderWizard() override;

    QUrl davUrl() const;

    QString destination() const;

    QString displayName() const;

    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    AccountPtr _account;
    class SpacesPage *_spacesPage;
    FolderWizardLocalPath *_folderWizardSourcePage = nullptr;
    FolderWizardRemotePath *_folderWizardTargetPage = nullptr;
    FolderWizardSelectiveSync *_folderWizardSelectiveSyncPage = nullptr;
};


} // namespace OCC

#endif
