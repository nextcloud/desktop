#include <QtCore>

#include "statusdialog.h"
#include "folder.h"


namespace Mirall {

StatusDialog::StatusDialog(QWidget *parent) :
    QDialog(parent)
{
    setupUi( this  );

    connect(_ButtonClose, SIGNAL(clicked()), this, SLOT(accept()));
    connect(_ButtonRemove, SIGNAL(clicked()), this, SLOT(slotRemoveFolder()));
}

void StatusDialog::setFolderList( QHash<QString, Folder*> folders )
{
    foreach( QString f, folders.keys() ) {
        qDebug() << "Folder: " << f;
    }
}

void StatusDialog::slotRemoveFolder()
{
    qDebug() << "Remove Folder!";
}

}

#include "statusdialog.moc"
