/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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
 */#include <QtCore>

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
