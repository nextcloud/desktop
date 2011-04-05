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
#include "mirall/unisonfolder.h"
#include "mirall/inotify.h"

namespace Mirall {

Application::Application(int argc, char **argv) :
    QApplication(argc, argv),
    _networkMgr(new QNetworkConfigurationManager(this)),
    _folderSyncCount(0)
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
    _tray->setIcon(QIcon(FOLDER_ICON));
    _tray->show();
}

void Application::setupContextMenu()
{
    QMenu *contextMenu = new QMenu();
    contextMenu->addAction(_actionAddFolder);

    // here all folders should be added
    foreach (Folder *folder, _folderMap) {
        contextMenu->addAction(folder->openAction());
    }

    contextMenu->addSeparator();

    contextMenu->addAction(_actionQuit);
    _tray->setContextMenu(contextMenu);
}

void Application::slotAddFolder()
{
    if (_folderWizard->exec() == QDialog::Accepted) {
        qDebug() << "* Folder wizard completed";
    }
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

            folder = new UnisonFolder(path.toString(),
                                      settings.value("backend:unison/secondPath").toString(),
                                      this);
        }
        else {
            qWarning() << "unknown backend" << backend;
            return;
        }
    }

    _folderMap[file] = folder;
    QObject::connect(folder, SIGNAL(syncStarted()), SLOT(slotFolderSyncStarted()));
    QObject::connect(folder, SIGNAL(syncFinished()), SLOT(slotFolderSyncFinished()));
}

void Application::slotFolderSyncStarted()
{
    _folderSyncCount++;

    if (_folderSyncCount > 0) {
        _tray->setIcon(QIcon(FOLDER_SYNC_ICON));
    }
}

void Application::slotFolderSyncFinished()
{
    _folderSyncCount--;

    if (_folderSyncCount < 1) {
        _tray->setIcon(QIcon(FOLDER_ICON));
    }
}


} // namespace Mirall

#include "application.moc"
