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
    _openAction = new QAction(QIcon(FOLDER_ICON), path, this);
    QObject::connect(_openAction, SIGNAL(triggered(bool)), SLOT(slotOpenFolder()));

    _watcher = new Mirall::FolderWatcher(path, this);
    QObject::connect(_watcher, SIGNAL(folderChanged(const QString &)),
                     SLOT(slotChanged(const QString &)));
}

QAction * Folder::openAction() const
{
    return _openAction;
}

Folder::~Folder()
{
}

void Folder::slotChanged(const QString &path)
{
    //qDebug() << "path " << path << " changed";
}

void Folder::slotOpenFolder()
{
    QDesktopServices::openUrl(QUrl(_path));
}


} // namespace Mirall

#include "folder.moc"
