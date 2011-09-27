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
#include <QHash>
#include <QSystemTrayIcon>

#include "mirall/syncresult.h"

class QAction;
class QMenu;
class QSystemTrayIcon;
class QNetworkConfigurationManager;

namespace Mirall {

class Folder;
class FolderWizard;
class StatusDialog;
class OwncloudWizard;

class Application : public QApplication
{
    Q_OBJECT
public:
    explicit Application(int argc, char **argv);
    ~Application();
signals:

protected slots:

    void slotAddFolder();

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

protected slots:
    void slotTrayClicked( QSystemTrayIcon::ActivationReason );

private:
    // configuration file -> folder
    QHash<QString, Folder *> _folderMap;
    QSystemTrayIcon *_tray;
    QAction *_actionQuit;
    QAction *_actionAddFolder;
    QNetworkConfigurationManager *_networkMgr;
    QString _folderConfigPath;

    // counter tracking number of folders doing a sync
    int _folderSyncCount;

    FolderWizard *_folderWizard;
    OwncloudWizard *_owncloudWizard;

    // tray's menu
    QMenu *_contextMenu;
    StatusDialog *_statusDialog;
};

} // namespace Mirall

#endif // APPLICATION_H
