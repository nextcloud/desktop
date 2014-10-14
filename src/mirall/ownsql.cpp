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


#include <QDateTime>
#include <QString>

#include "ownsql.h"

#define SQLITE_DO(A) if(1) { \
    _errId = (A); if(_errId != SQLITE_OK) { _error= QString::fromUtf8(sqlite3_errmsg(_db)); \
     } }

SqlDatabase::SqlDatabase()
    :_db(NULL)
{

}

bool SqlDatabase::isOpen()
{
    return _db != NULL;
}

bool SqlDatabase::open( const QString& filename )
{
    if(isOpen()) {
        return true;
    }

    int flag = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_NOMUTEX;
    SQLITE_DO( sqlite3_open_v2(filename.toUtf8().constData(), &_db, flag, NULL) );

    if( _errId != SQLITE_OK ) {
        close(); // FIXME: Correct?
        _db = NULL;
    }
    return isOpen();
}

QString SqlDatabase::error() const
{
    const QString err(_error);
    // _error.clear();
    return err;
}

void SqlDatabase::close()
{
    if( _db ) {
        SQLITE_DO(sqlite3_close_v2(_db) );
    }
}

bool SqlDatabase::transaction()
{
    return true;
}

bool SqlDatabase::commit()
{
    return true;
}

sqlite3* SqlDatabase::sqliteDb()
{
    return _db;
}

/* =========================================================================================== */

SqlQuery::SqlQuery( SqlDatabase db )
    :_db(db.sqliteDb())
{

}

SqlQuery::~SqlQuery()
{
    if( _stmt ) {
        sqlite3_finalize(_stmt);
    }
}

SqlQuery::SqlQuery(const QString& sql, SqlDatabase db)
    :_db(db.sqliteDb())
{
    prepare(sql);
}

void SqlQuery::prepare( const QString& sql)
{
    SQLITE_DO(sqlite3_prepare_v2(_db, sql.toUtf8().constData(), -1, &_stmt, NULL));
}

bool SqlQuery::exec()
{
    SQLITE_DO(sqlite3_step(_stmt));

    return (_errId == SQLITE_ROW || _errId == SQLITE_DONE);
}

void SqlQuery::bindValue(int pos, const QVariant& value)
{
    int res;
    if( _stmt ) {
        switch (value.type()) {
        case QVariant::Int:
        case QVariant::Bool:
            res = sqlite3_bind_int(_stmt, pos, value.toInt());
            break;
        case QVariant::Double:
            res = sqlite3_bind_double(_stmt, pos, value.toDouble());
            break;
        case QVariant::UInt:
        case QVariant::LongLong:
            res = sqlite3_bind_int64(_stmt, pos, value.toLongLong());
            break;
        case QVariant::DateTime: {
            const QDateTime dateTime = value.toDateTime();
            const QString str = dateTime.toString(QLatin1String("yyyy-MM-ddThh:mm:ss.zzz"));
            res = sqlite3_bind_text16(_stmt, pos, str.utf16(),
                                      str.size() * sizeof(ushort), SQLITE_TRANSIENT);
            break;
        }
        case QVariant::Time: {
            const QTime time = value.toTime();
            const QString str = time.toString(QLatin1String("hh:mm:ss.zzz"));
            res = sqlite3_bind_text16(_stmt, pos, str.utf16(),
                                      str.size() * sizeof(ushort), SQLITE_TRANSIENT);
            break;
        }
        case QVariant::String: {
            // lifetime of string == lifetime of its qvariant
            const QString *str = static_cast<const QString*>(value.constData());
            res = sqlite3_bind_text16(_stmt, pos, str->utf16(),
                                      (str->size()) * sizeof(QChar), SQLITE_STATIC);
            break; }
        default: {
            QString str = value.toString();
            // SQLITE_TRANSIENT makes sure that sqlite buffers the data
            res = sqlite3_bind_text16(_stmt, pos, str.utf16(),
                                      (str.size()) * sizeof(QChar), SQLITE_TRANSIENT);
            break; }
        }
    }
}

QString SqlQuery::stringValue(int index)
{
    return QString::fromUtf16(static_cast<const ushort*>(sqlite3_column_text16(_stmt, index)));
}

int SqlQuery::intValue(int index)
{
    return sqlite3_column_int(_stmt, index);
}

quint64 SqlQuery::int64Value(int index)
{
    return sqlite3_column_int64(_stmt, index);
}

QByteArray SqlQuery::baValue(int index)
{
    return QByteArray( static_cast<const char*>(sqlite3_column_blob(_stmt, index)),
                       sqlite3_column_bytes(_stmt, index));
}

bool SqlQuery::next()
{
    SQLITE_DO(sqlite3_step(_stmt));
    return _errId == SQLITE_ROW;
}

QString SqlQuery::error() const
{
    return QString("ERROR - not yet implemented");
}

QString SqlQuery::lastQuery() const
{
    return QString("Last Query");
}

int SqlQuery::numRowsAffected()
{
    return 1;
}

void SqlQuery::finish()
{
    SQLITE_DO(sqlite3_finalize(_stmt));
    _stmt = NULL;
}
