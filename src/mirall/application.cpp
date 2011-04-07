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

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QNetworkConfigurationManager>
#include <QSettings>
#include <QStringList>
#include <QSystemTrayIcon>

#include "mirall/constants.h"
#include "mirall/application.h"
#include "mirall/folder.h"
#include "mirall/folderwizard.h"
#include "mirall/networklocation.h"
#include "mirall/unisonfolder.h"
#ifdef WITH_CSYNC
#include "mirall/csyncfolder.h"
#endif
#include "mirall/inotify.h"

namespace Mirall {

Application::Application(int argc, char **argv) :
    QApplication(argc, argv),
    _networkMgr(new QNetworkConfigurationManager(this)),
    _folderSyncCount(0),
    _contextMenu(0)
{
    INotify::initialize();

    setApplicationName("Mirall");
    setQuitOnLastWindowClosed(false);

    _folderWizard = new FolderWizard();

    _folderConfigPath = QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/folders";

    setupActions();
    setupSystemTray();

    qDebug() << "* Network is" << (_networkMgr->isOnline() ? "online" : "offline");
    foreach (QNetworkConfiguration netCfg, _networkMgr->allConfigurations(QNetworkConfiguration::Active)) {
        //qDebug() << "Network:" << netCfg.identifier();
    }

    // if QDir::mkpath would not be so stupid, I would not need to have this
    // duplication of folderConfigPath() here
    QDir storageDir(QDesktopServices::storageLocation(QDesktopServices::DataLocation));
    storageDir.mkpath("folders");

    setupKnownFolders();

    setupContextMenu();

    qDebug() << NetworkLocation::currentLocation().encoded();
}

Application::~Application()
{
    qDebug() << "* Mirall shutdown";
    INotify::cleanup();

    delete _networkMgr;
    delete _tray;

    foreach (Folder *folder, _folderMap) {
        delete folder;
    }
}

QString Application::folderConfigPath() const
{
    return _folderConfigPath;
}

void Application::setupActions()
{
    _actionAddFolder = new QAction(tr("Add folder"), this);
    QObject::connect(_actionAddFolder, SIGNAL(triggered(bool)), SLOT(slotAddFolder()));
    _actionQuit = new QAction(tr("Quit"), this);
    QObject::connect(_actionQuit, SIGNAL(triggered(bool)), SLOT(quit()));
}

void Application::setupSystemTray()
{
    _tray = new QSystemTrayIcon(this);
    _tray->setIcon(QIcon::fromTheme(FOLDER_ICON));
    _tray->show();
}

void Application::setupContextMenu()
{
    delete _contextMenu;
    _contextMenu = new QMenu();
    _contextMenu->addAction(_actionAddFolder);

    // here all folders should be added
    foreach (Folder *folder, _folderMap) {
        _contextMenu->addAction(folder->openAction());
    }

    _contextMenu->addSeparator();

    _contextMenu->addAction(_actionQuit);
    _tray->setContextMenu(_contextMenu);
}

void Application::slotAddFolder()
{
    if (_folderWizard->exec() == QDialog::Accepted) {
        qDebug() << "* Folder wizard completed";

        QString alias = _folderWizard->field("alias").toString();

        QSettings settings(folderConfigPath() + "/" + alias, QSettings::IniFormat);
        settings.setValue("folder/backend", "unison");
        settings.setValue("folder/path", _folderWizard->field("sourceFolder"));

        if (_folderWizard->field("local?").toBool()) {
            settings.setValue("backend:unison/secondPath", _folderWizard->field("targetLocalFolder"));
        }
        else if (_folderWizard->field("remote?").toBool()) {
            settings.setValue("backend:unison/secondPath", _folderWizard->field("targetSSHFolder"));
            bool onlyOnline = _folderWizard->field("onlyOnline?").toBool();
            settings.setValue("folder/onlyOnline", onlyOnline);

            if (onlyOnline) {
                bool onlyThisLAN = _folderWizard->field("onlyThisLAN?").toBool();
                settings.setValue("folder/onlyThisLAN", onlyThisLAN);
                if (onlyThisLAN) {
                    settings.setValue("folder/onlyOnline", true);
                }
            }
        }
        else
        {
            qWarning() << "* Folder not local and note remote?";
            return;
        }

        settings.sync();
        setupFolderFromConfigFile(alias);
        setupContextMenu();
    }
    else
        qDebug() << "* Folder wizard cancelled";
}

void Application::setupKnownFolders()
{
    qDebug() << "* Setup folders from " << folderConfigPath();

    QDir dir(folderConfigPath());
    dir.setFilter(QDir::Files);
    QStringList list = dir.entryList();
    foreach (QString file, list) {
        setupFolderFromConfigFile(file);
    }
}

// filename is the name of the file only, it does not include
// the configuration directory path
void Application::setupFolderFromConfigFile(const QString &file) {
    Folder *folder = 0L;

    qDebug() << "  ` -> setting up:" << file;
    QSettings settings(folderConfigPath() + "/" + file, QSettings::IniFormat);

    if (!settings.contains("folder/path")) {
        qWarning() << "    `->" << file << "is not a valid folder configuration";
        return;
    }

    QVariant path = settings.value("folder/path").toString();
    if (path.isNull() || !QFileInfo(path.toString()).isDir()) {
        qWarning() << "    `->" << path.toString() << "does not exist. Skipping folder" << file;
        _tray->showMessage(tr("Unknown folder"),
                           tr("Folder %1 does not exist").arg(path.toString()),
                           QSystemTrayIcon::Critical);
        return;
    }

    QVariant backend = settings.value("folder/backend");
    if (!backend.isNull()) {
        if (backend.toString() == "unison") {
            folder = new UnisonFolder(file,
                                      path.toString(),
                                      settings.value("backend:unison/secondPath").toString(),
                                      this);
        }
        else if (backend.toString() == "csync") {
#ifdef WITH_CSYNC
            folder = new CSyncFolder(file,
                                     path.toString(),
                                     settings.value("backend:csync/secondPath").toString(),
                                     this);
#else
            qCritical() << "* csync suport not enabled!! ignoring:" << file;
#endif
        }
        else {
            qWarning() << "unknown backend" << backend;
            return;
        }
    }
    folder->setOnlyOnlineEnabled(settings.value("folder/onlyOnline", false).toBool());
    folder->setOnlyThisLANEnabled(settings.value("folder/onlyThisLAN", false).toBool());

    _folderMap[file] = folder;
    QObject::connect(folder, SIGNAL(syncStarted()), SLOT(slotFolderSyncStarted()));
    QObject::connect(folder, SIGNAL(syncFinished()), SLOT(slotFolderSyncFinished()));
}

void Application::slotFolderSyncStarted()
{
    _folderSyncCount++;

    if (_folderSyncCount > 0) {
        _tray->setIcon(QIcon::fromTheme(FOLDER_SYNC_ICON));
    }
}

void Application::slotFolderSyncFinished()
{
    _folderSyncCount--;

    if (_folderSyncCount < 1) {
        _tray->setIcon(QIcon::fromTheme(FOLDER_ICON));
    }
}


} // namespace Mirall

#include "application.moc"
