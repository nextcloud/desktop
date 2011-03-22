#include <QDebug>
#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

#include "mirall/constants.h"
#include "mirall/application.h"
#include "mirall/folder.h"
#include "mirall/unisonfolder.h"
#include "mirall/inotify.h"

namespace Mirall {

Application::Application(int argc, char **argv) :
    QApplication(argc, argv)
{
    INotify::initialize();

    _folder = new UnisonFolder(QDir::homePath() + "/Mirall", "/space/tmp/testmirall", this);
    setApplicationName("Mirall");
    setupActions();
    setupSystemTray();
    setupContextMenu();
}

Application::~Application()
{
    INotify::cleanup();
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
    contextMenu->addAction(_folder->openAction());

    contextMenu->addSeparator();

    contextMenu->addAction(_actionQuit);
    _tray->setContextMenu(contextMenu);
}

void Application::slotAddFolder()
{
    qDebug() << "add a folder here...";
}


} // namespace Mirall

#include "application.moc"
