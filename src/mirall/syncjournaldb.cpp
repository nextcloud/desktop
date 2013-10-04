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

bool SyncJournalDb::exists()
{
    QMutexLocker locker(&_mutex);
    return (!_dbFile.isEmpty() && QFile::exists(_dbFile));
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


    QSqlQuery createQuery("CREATE TABLE IF NOT EXISTS metadata("
                          "phash INTEGER(8),"
                          "pathlen INTEGER,"
                          "path VARCHAR(4096),"
                          "inode INTEGER,"
                          "uid INTEGER,"
                          "gid INTEGER,"
                          "mode INTEGER,"
                          "modtime INTEGER(8),"
                          "type INTEGER,"
                          "md5 VARCHAR(32),"
                          "PRIMARY KEY(phash)"
                          ");" , _db);

    if (!createQuery.exec()) {
        qWarning() << "Error creating table metadata : " << createQuery.lastError().text();
        return false;
    }

    return true;
}

qint64 SyncJournalDb::getPHash(const QString& file) const
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
    QMutexLocker locker(&_mutex);
    qlonglong phash = getPHash(record._path);

    if( checkConnect() ) {

        QSqlQuery writeQuery( "INSERT OR REPLACE INTO metadata "
                              "(phash, pathlen, path, inode, uid, gid, mode, modtime, type, md5) "
                              "VALUES ( ? , ?, ? , ? , ? , ? , ?,  ? , ? , ? )", _db );
//                              "VALUES ( :phash , :plen, :path , :inode , :uid , :gid , :mode,  :modtime , :type , :etag )" );


        QByteArray arr = record._path.toUtf8();
        int plen = arr.length();

//         writeQuery.bindValue(":phash",   QString::number(phash));
//         writeQuery.bindValue(":plen",    plen);
//         writeQuery.bindValue(":path",    record._path );
//         writeQuery.bindValue(":inode",   record._inode );
//         writeQuery.bindValue(":uid",     record._uid );
//         writeQuery.bindValue(":gid",     record._gid );
//         writeQuery.bindValue(":mode",    record._mode );
//         writeQuery.bindValue(":modtime", QString::number(record._modtime.toTime_t()));
//         writeQuery.bindValue(":type",    QString::number(record._type) );
//         writeQuery.bindValue(":etag",    record._etag );
        writeQuery.bindValue(0, QString::number(phash));
        writeQuery.bindValue(1, plen);
        writeQuery.bindValue(2, record._path );
        writeQuery.bindValue(3, record._inode );
        writeQuery.bindValue(4, record._uid );
        writeQuery.bindValue(5, record._gid );
        writeQuery.bindValue(6, record._mode );
        writeQuery.bindValue(7, QString::number(record._modtime.toTime_t()));
        writeQuery.bindValue(8, QString::number(record._type) );
        writeQuery.bindValue(9, record._etag );


        if( !writeQuery.exec() ) {
            qWarning() << "Exec error of SQL statement: " << writeQuery.lastQuery() <<  " :"   << writeQuery.lastError().text();
            return false;
        }

        qDebug() <<  writeQuery.lastQuery() << phash << plen << record._path << record._inode
                 << record._uid << record._gid << record._mode
                 << QString::number(record._modtime.toTime_t()) << QString::number(record._type)
                 << record._etag;

        return true;
    } else {
        qDebug() << "Failed to connect database.";
        return false; // checkConnect failed.
    }
}

bool SyncJournalDb::deleteFileRecord(const QString& filename)
{
    QMutexLocker locker(&_mutex);
    qlonglong phash = getPHash(filename);

    if( checkConnect() ) {

        QSqlQuery query( "DELETE FROM metadata WHERE phash=?" );
        query.bindValue( 0, QString::number(phash) );

        if( !query.exec() ) {
            qWarning() << "Exec error of SQL statement: " << query.lastQuery() <<  " : " << query.lastError().text();
            return false;
        }
        qDebug() <<  query.executedQuery() << phash << filename;
        return true;
    } else {
        qDebug() << "Failed to connect database.";
        return false; // checkConnect failed.
    }
}


SyncJournalFileRecord SyncJournalDb::getFileRecord( const QString& filename )
{
    QMutexLocker locker(&_mutex);

    qlonglong phash = getPHash( filename );
    SyncJournalFileRecord rec;

    /*
    CREATE TABLE "metadata"(phash INTEGER(8),pathlen INTEGER,path VARCHAR(4096),inode INTEGER,uid INTEGER,gid INTEGER,mode INTEGER,modtime INTEGER(8),type INTEGER,md5 VARCHAR(32),PRIMARY KEY(phash));
    CREATE INDEX metadata_inode ON metadata(inode);
    CREATE INDEX metadata_phash ON metadata(phash);
    */

    if( checkConnect() ) {
        QSqlQuery query("SELECT path, inode, uid, gid, mode, modtime, type, md5 FROM "
                        "metadata WHERE phash=:ph" ,  _db);
        query.bindValue(":ph", QString::number(phash));

        if (!query.exec()) {
            QString err = query.lastError().text();
            qDebug() << "Error creating prepared statement: " << query.lastQuery() << ", Error:" << err;;
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
            qDebug() << "Can not query " << query.lastQuery() << ", Error:" << err;
        }
    }
    return rec;
}

int SyncJournalDb::getFileRecordCount()
{
    if( !checkConnect() )
        return 0;

    QSqlQuery query("SELECT COUNT(*) FROM metadata" ,  _db);

    if (!query.exec()) {
        QString err = query.lastError().text();
        qDebug() << "Error creating prepared statement: " << query.lastQuery() << ", Error:" << err;;
        return 0;
    }

    if (query.next()) {
        int count = query.value(0).toInt();
        return count;
    }

    return 0;
}


} // namespace Mirall
