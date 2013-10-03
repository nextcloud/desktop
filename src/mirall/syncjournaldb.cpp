/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QFile>
#include <QStringList>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

#include <c_jhash.h>
#include <inttypes.h>

#include "syncjournaldb.h"
#include "syncjournalfilerecord.h"

#define QSQLITE "QSQLITE"

namespace Mirall {

SyncJournalDb::SyncJournalDb(const QString& path, QObject *parent) :
    QObject(parent)
{

    _dbFile = path;
    if( !_dbFile.endsWith('/') ) {
        _dbFile.append('/');
    }
    _dbFile.append(".csync_journal.db");
}

bool SyncJournalDb::checkConnect()
{
    if( _db.isOpen() ) {
        return true;
    }

    if( _dbFile.isEmpty() || !QFile::exists(_dbFile) ) {
        return false;
    }

    QStringList list = QSqlDatabase::drivers();
    if( list.size() == 0 ) {
        qDebug() << "Database Drivers could not be loaded.";
        return false ;
    } else {
        if( list.indexOf( QSQLITE ) == -1 ) {
            qDebug() << "Database Driver QSQLITE could not be loaded!";
            return false;
        }
    }

    _db = QSqlDatabase::addDatabase( QSQLITE );
    _db.setDatabaseName(_dbFile);

    if (!_db.isOpen()) {
        if( !_db.open() ) {
            QSqlError error = _db.lastError();
            qDebug() << "Error opening the db: " << error.text();
            return false;
        }
    }

    return true;
}

int64_t SyncJournalDb::getPHash(const QString& file) const
{
    QByteArray utf8File = file.toUtf8();
    int64_t h;

    if( file.isEmpty() ) {
        return -1;
    }

    int len = utf8File.length();

    h = c_jhash64((uint8_t *) utf8File.data(), len, 0);
    return h;
}

bool SyncJournalDb::setFileRecord( const SyncJournalFileRecord& record )
{
    int64_t phash = getPHash(record._path);

    if( checkConnect() ) {

        QSqlQuery query( "SELECT phash FROM metadata WHERE phash=:phash" );

        query.bindValue( ":phash", QVariant::fromValue(phash) );

        bool haveEntry = false;
        if( query.next() ) {
            haveEntry = true;
        }

        QString sql;

        if( haveEntry ) {
            sql = "UPDATE metadata ";
            sql += "(pathlen, path, inode, uid, gid, mode, modtime, type, md5) "
                    "VALUES (:plen, :path, :inode, :uid, :gid, :mode, :modtime, :type: :etag) "
                    "WHERE phash=:phash";
        } else {
            sql = "INSERT INTO metadata ";
            sql += "(phash, pathlen, path, inode, uid, gid, mode, modtime, type, md5) "
                    "VALUES (:phash, :plen, :path, :inode, :uid, :gid, :mode, :modtime, :type: :etag)";
        }

        QSqlQuery writeQuery(sql);

        QByteArray arr = record._path.toUtf8();
        int plen = arr.length();

        writeQuery.bindValue(":phash",   QVariant::fromValue(phash));
        writeQuery.bindValue(":plen",    plen);
        writeQuery.bindValue(":path",    record._path );
        writeQuery.bindValue(":inode",   record._inode );
        writeQuery.bindValue(":uid",     record._uid );
        writeQuery.bindValue(":gid",     record._gid );
        writeQuery.bindValue(":mode",    record._mode );
        writeQuery.bindValue(":modtime", record._modtime.toTime_t());
        writeQuery.bindValue(":type",    record._type );
        writeQuery.bindValue(":etag",    record._etag );

        if( !writeQuery.exec() ) {
            qDebug() << "Exec error of SQL statement: " << writeQuery.lastError().text();
            return false;
        }
        return true;
    } else {
        qDebug() << "Failed to connect database.";
        return false; // checkConnect failed.
    }
}

SyncJournalFileRecord SyncJournalDb::getFileRecord( const QString& filename )
{
    qlonglong phash = getPHash( filename );
    SyncJournalFileRecord rec;

    /*
    CREATE TABLE "metadata"(phash INTEGER(8),pathlen INTEGER,path VARCHAR(4096),inode INTEGER,uid INTEGER,gid INTEGER,mode INTEGER,modtime INTEGER(8),type INTEGER,md5 VARCHAR(32),PRIMARY KEY(phash));
    CREATE INDEX metadata_inode ON metadata(inode);
    CREATE INDEX metadata_phash ON metadata(phash);
    */

    if( checkConnect() ) {
        QSqlQuery query("SELECT path, inode, uid, gid, mode, modtime, type, md5 FROM "
                        "metadata WHERE phash=:ph");
        query.bindValue(":ph", QString::number(phash));

        if (!query.exec()) {
            QString err = query.lastError().text();
            qDebug() << "Error creating prepared statement: " << err;
            return rec;
        }

        if( query.next() ) {
            bool ok;
            rec._path    = query.value(0).toString();
            rec._inode   = query.value(1).toInt(&ok);
            rec._uid     = query.value(2).toInt(&ok);
            rec._gid     = query.value(3).toInt(&ok);
            rec._mode    = query.value(4).toInt(&ok);
            uint mtime   = query.value(5).toUInt(&ok);
            rec._modtime = QDateTime::fromTime_t( mtime );
            rec._type    = query.value(6).toInt(&ok);
            rec._etag    = query.value(7).toString();
        } else {
            QString err = query.lastError().text();
            qDebug() << "Can not query, Error: " << err;
        }
    }
    return rec;
}


} // namespace Mirall
