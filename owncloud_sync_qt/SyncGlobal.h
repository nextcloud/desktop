/******************************************************************************
 *    Copyright 2011 Juan Carlos Cornejo jc2@paintblack.com
 *
 *    This file is part of owncloud_sync_qt.
 *
 *    owncloud_sync is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    owncloud_sync is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with owncloud_sync.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/
#ifndef SYNCGLOBAL_H
#define SYNCGLOBAL_H

#include <QIODevice>
#include <QDebug>

#define _OCS_VERSION "0.5.3"
#define _OCS_DB_VERSION 2

/*! \brief An internal OwnCloud Sync Qt debugging class.
  * May be used like the normal Qt qDebug() like so:
  * syncDebug() << "Some debugging code"
  */
class SyncDebug : public QIODevice
{
    Q_OBJECT
public:
    explicit SyncDebug( QObject *parent = 0 ): QIODevice(parent) {
        open(ReadWrite);
    }

    qint64 readData(char *data, qint64 length ) {
        return 0;
    }

    qint64 writeData(const char* data, qint64 length){
        qDebug() << data;
        emit debugMessage(QString(data));
        return 0;
    }

signals:
    void debugMessage(const QString);

public slots:

};

#if !defined(QT_NO_DEBUG_STREAM)
Q_GLOBAL_STATIC( SyncDebug, getSyncDebug)
Q_CORE_EXPORT_INLINE QDebug syncDebug() { return QDebug(getSyncDebug()); }

#else // QT_NO_DEBUG_STREAM

#endif

#endif // SYNCGLOBAL_H
