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


    QSqlQuery createQuery1("CREATE TABLE IF NOT EXISTS metadata("
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

    if (!createQuery1.exec()) {
        qWarning() << "Error creating table metadata : " << createQuery1.lastError().text();
        return false;
    }

    QSqlQuery createQuery2("CREATE TABLE IF NOT EXISTS downloadinfo("
                           "path VARCHAR(4096),"
                           "tmpfile VARCHAR(4096),"
                           "etag VARCHAR(32),"
                           "errorcount INTEGER,"
                           "PRIMARY KEY(path)"
                           ");" , _db);

    if (!createQuery2.exec()) {
        qWarning() << "Error creating table downloadinfo : " << createQuery2.lastError().text();
        return false;
    }

    QSqlQuery createQuery3("CREATE TABLE IF NOT EXISTS uploadinfo("
                           "path VARCHAR(4096),"
                           "chunk INTEGER,"
                           "transferid INTEGER,"
                           "errorcount INTEGER,"
                           "size INTEGER(8),"
                           "modtime INTEGER(8),"
                           "PRIMARY KEY(path)"
                           ");" , _db);

    if (!createQuery3.exec()) {
        qWarning() << "Error creating table downloadinfo : " << createQuery3.lastError().text();
        return false;
    }

    bool rc = updateDatabaseStructure();

    return rc;
}

bool SyncJournalDb::updateDatabaseStructure()
{
    QStringList columns = tableColumns("metadata");

    // check if the file_id column is there and create it if not
    if( columns.indexOf(QLatin1String("fileid")) == -1 ) {
        QSqlQuery addFileIdColQuery("ALTER TABLE metadata ADD COLUMN fileid VARCHAR(128);", _db);
        addFileIdColQuery.exec();
        QSqlQuery indx("CREATE INDEX metadata_file_id ON metadata(fileid);", _db);
        indx.exec();
    }
    return true;
}

QStringList SyncJournalDb::tableColumns( const QString& table )
{
    QStringList columns;
    if( !table.isEmpty() ) {

        QString q = QString("PRAGMA table_info(%1);").arg(table);
        QSqlQuery query(q, _db);

        if(!query.exec()) {
            QString err = query.lastError().text();
            qDebug() << "Error creating prepared statement: " << query.lastQuery() << ", Error:" << err;;
            return columns;
        }

        while( query.next() ) {
            columns.append( query.value(1).toString() );
        }
        query.finish();
    }
    qDebug() << "Columns in the current journal: " << columns;

    return columns;
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
                              "(phash, pathlen, path, inode, uid, gid, mode, modtime, type, md5, fileid) "
                              "VALUES ( ? , ?, ? , ? , ? , ? , ?,  ? , ? , ?, ? )", _db );

        QByteArray arr = record._path.toUtf8();
        int plen = arr.length();

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
        writeQuery.bindValue(10, record._fileId );

        if( !writeQuery.exec() ) {
            qWarning() << "Exec error of SQL statement: " << writeQuery.lastQuery() <<  " :"
                       << writeQuery.lastError().text();
            return false;
        }

        qDebug() <<  writeQuery.lastQuery() << phash << plen << record._path << record._inode
                 << record._uid << record._gid << record._mode
                 << QString::number(record._modtime.toTime_t()) << QString::number(record._type)
                 << record._etag << record._fileId;

        return true;
    } else {
        qDebug() << "Failed to connect database.";
        return false; // checkConnect failed.
    }
}

bool SyncJournalDb::deleteFileRecord(const QString& filename, bool recursively)
{
    QMutexLocker locker(&_mutex);

    if( checkConnect() ) {
        if (recursively) {
            qlonglong phash = getPHash(filename);
            QSqlQuery query( "DELETE FROM metadata WHERE phash=?", _db );
            query.bindValue( 0, QString::number(phash) );

            if( !query.exec() ) {
                qWarning() << "Exec error of SQL statement: " << query.lastQuery() <<  " : " << query.lastError().text();
                return false;
            }
            qDebug() <<  query.executedQuery() << phash << filename;
            return true;
        } else {
            QSqlQuery query( "DELETE FROM metadata WHERE path LIKE(?||'/%')", _db );
            query.bindValue( 0, filename );

            if( !query.exec() ) {
                qWarning() << "Exec error of SQL statement: " << query.lastQuery() <<  " : " << query.lastError().text();
                return false;
            }
            qDebug() <<  query.executedQuery()  << filename;
            return true;
        }
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
        QSqlQuery query("SELECT path, inode, uid, gid, mode, modtime, type, md5, fileid FROM "
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
            rec._modtime = QDateTime::fromTime_t(query.value(5).toLongLong(&ok));
            rec._type    = query.value(6).toInt(&ok);
            rec._etag    = query.value(7).toString();
            rec._fileId  = query.value(8).toString();
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

SyncJournalDb::DownloadInfo SyncJournalDb::getDownloadInfo(const QString& file)
{
    QMutexLocker locker(&_mutex);

    DownloadInfo res;

    if( checkConnect() ) {
        QSqlQuery query("SELECT tmpfile, etag, errorcount FROM "
                        "downloadinfo WHERE path=:pa" , _db);
        query.bindValue(":pa", file);

        if (!query.exec()) {
            QString err = query.lastError().text();
            qDebug() << "Database error for file " << file << " : " << query.lastQuery() << ", Error:" << err;;
            return res;
        }

        if( query.next() ) {
            bool ok = true;
            res._tmpfile = query.value(0).toString();
            res._etag    = query.value(1).toByteArray();
            res._errorCount = query.value(2).toInt(&ok);
            res._valid   = ok;
        }
    }
    return res;
}

void SyncJournalDb::setDownloadInfo(const QString& file, const SyncJournalDb::DownloadInfo& i)
{
    QMutexLocker locker(&_mutex);

    if( !checkConnect() )
        return;

    if (i._valid) {

        QSqlQuery writeQuery( "INSERT OR REPLACE INTO downloadinfo "
                              "(path, tmpfile, etag, errorcount) "
                              "VALUES ( ? , ?, ? , ? )", _db );

        writeQuery.bindValue(0, file);
        writeQuery.bindValue(1, i._tmpfile);
        writeQuery.bindValue(2, i._etag );
        writeQuery.bindValue(3, i._errorCount );

        if( !writeQuery.exec() ) {
            qWarning() << "Exec error of SQL statement: " << writeQuery.lastQuery() <<  " :"   << writeQuery.lastError().text();
            return;
        }

        qDebug() <<  writeQuery.lastQuery() << file << i._tmpfile << i._etag << i._errorCount;
    } else {
        QSqlQuery query( "DELETE FROM downloadinfo WHERE path=?" );
        query.bindValue( 0, file );

        if( !query.exec() ) {
            qWarning() << "Exec error of SQL statement: " << query.lastQuery() <<  " : " << query.lastError().text();
            return;
        }
        qDebug() <<  query.executedQuery()  << file;
    }
}

SyncJournalDb::UploadInfo SyncJournalDb::getUploadInfo(const QString& file)
{
    QMutexLocker locker(&_mutex);

    UploadInfo res;

    if( checkConnect() ) {
        QSqlQuery query("SELECT chunk, transferid, errorcount, size, modtime FROM "
                        "uploadinfo WHERE path=:pa" , _db);
        query.bindValue(":pa", file);

        if (!query.exec()) {
            QString err = query.lastError().text();
            qDebug() << "Database error for file " << file << " : " << query.lastQuery() << ", Error:" << err;
            return res;
        }

        if( query.next() ) {
            bool ok = true;
            res._chunk      = query.value(0).toInt(&ok);
            res._transferid = query.value(1).toInt(&ok);
            res._errorCount = query.value(2).toInt(&ok);
            res._size       = query.value(3).toLongLong(&ok);
            res._modtime    = QDateTime::fromTime_t(query.value(4).toLongLong(&ok));
            res._valid      = ok;
        }
    }
    return res;
}

void SyncJournalDb::setUploadInfo(const QString& file, const SyncJournalDb::UploadInfo& i)
{
    QMutexLocker locker(&_mutex);

    if( !checkConnect() )
        return;

    if (i._valid) {

        QSqlQuery writeQuery( "INSERT OR REPLACE INTO uploadinfo "
                              "(path, chunk, transferid, errorcount, size, modtime) "
                              "VALUES ( ? , ?, ? , ? ,  ? , ? )", _db );

        writeQuery.bindValue(0, file);
        writeQuery.bindValue(1, i._chunk);
        writeQuery.bindValue(2, i._transferid );
        writeQuery.bindValue(3, i._errorCount );
        writeQuery.bindValue(4, i._size );
        writeQuery.bindValue(5, QString::number(i._modtime.toTime_t()) );

        if( !writeQuery.exec() ) {
            qWarning() << "Exec error of SQL statement: " << writeQuery.lastQuery() <<  " :"   << writeQuery.lastError().text();
            return;
        }

        qDebug() <<  writeQuery.lastQuery() << file << i._chunk << i._transferid << i._errorCount;
    } else {
        QSqlQuery query( "DELETE FROM uploadinfo WHERE path=?" );
        query.bindValue( 0, file );

        if( !query.exec() ) {
            qWarning() << "Exec error of SQL statement: " << query.lastQuery() <<  " : " << query.lastError().text();
            return;
        }
        qDebug() <<  query.executedQuery() << file;
    }
}


} // namespace Mirall
