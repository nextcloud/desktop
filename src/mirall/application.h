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

#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QSystemTrayIcon>

#include "mirall/syncresult.h"
#include "mirall/folder.h"

class QAction;
class QMenu;
class QSystemTrayIcon;
class QNetworkConfigurationManager;

namespace Mirall {
class Theme;
class FolderWatcher;
class FolderWizard;
class StatusDialog;
class OwncloudSetup;

class Application : public QApplication
{
    Q_OBJECT
public:
    explicit Application(int argc, char **argv);
    ~Application();
signals:

protected slots:

    void slotReparseConfiguration();
    void slotAddFolder();
    void slotRemoveFolder( const QString& );
    void slotFetchFolder( const QString& );
    void slotPushFolder( const QString& );
    void slotEnableFolder( const QString&, const bool );
    void slotInfoFolder( const QString& );
    void slotConfigure();

    void slotFolderSyncStarted();
    void slotFolderSyncFinished(const SyncResult &);

protected:

    QString folderConfigPath() const;

    void setupActions();
    void setupSystemTray();
    void setupContextMenu();

    // finds all folder configuration files
    // and create the folders
    void setupKnownFolders();

    // creates a folder for a specific
    // configuration
    void setupFolderFromConfigFile(const QString &filename);

    //folders have to be disabled while making config changes
    void disableFoldersWithRestore();
    void restoreEnabledFolders();
    void computeOverallSyncStatus();

protected slots:
    void slotTrayClicked( QSystemTrayIcon::ActivationReason );

private:
    // configuration file -> folder
    Folder::Map _folderMap;
    QSystemTrayIcon *_tray;
    QAction *_actionQuit;
    QAction *_actionAddFolder;
    QAction *_actionConfigure;

    QNetworkConfigurationManager *_networkMgr;
    QString _folderConfigPath;

    // counter tracking number of folders doing a sync
    int _folderSyncCount;

    FolderWatcher *_configFolderWatcher;
    FolderWizard  *_folderWizard;
    OwncloudSetup *_owncloudSetup;

    // tray's menu
    QMenu *_contextMenu;
    StatusDialog *_statusDialog;

    QHash<QString, bool> _folderEnabledMap;

    Theme *_theme;
};

} // namespace Mirall

#endif // APPLICATION_H
