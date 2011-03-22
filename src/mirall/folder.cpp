#include <QAction>
#include <QDebug>
#include <QDesktopServices>
#include <QIcon>
#include <QMutexLocker>
#include <QUrl>

#include "mirall/constants.h"
#include "mirall/folder.h"
#include "mirall/folderwatcher.h"

namespace Mirall {

Folder::Folder(const QString &path, QObject *parent)
    : QObject(parent),
      _path(path)
{
    _action = new QAction(QIcon(FOLDER_ICON), path, this);
    QObject::connect(_action, SIGNAL(triggered(bool)), SLOT(slotOpenFolder()));

    _watcher = new Mirall::FolderWatcher(path, this);
    QObject::connect(_watcher, SIGNAL(folderChanged(const QStringList &)),
                     SLOT(slotChanged(const QStringList &)));
}

Folder::~Folder()
{
}

QString Folder::path() const
{
    return _path;
}

QAction * Folder::action() const
{
    return _action;
}

void Folder::slotChanged(const QStringList &pathList)
{
    qDebug() << "Changed >> " << pathList;

}

void Folder::slotOpenFolder()
{
    QDesktopServices::openUrl(QUrl(_path));
}


} // namespace Mirall

#include "folder.moc"
