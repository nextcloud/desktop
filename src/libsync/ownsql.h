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

#ifndef OWNSQL_H
#define OWNSQL_H

#include <sqlite3.h>

#include <QObject>
#include <QVariant>

#include "owncloudlib.h"

namespace OCC {

class OWNCLOUDSYNC_EXPORT SqlDatabase
{
    Q_DISABLE_COPY(SqlDatabase)
public:
    explicit SqlDatabase();

    bool isOpen();
    bool openOrCreateReadWrite( const QString& filename );
    bool openReadOnly( const QString& filename );
    bool transaction();
    bool commit();
    void close();
    QString error() const;
    sqlite3* sqliteDb();

private:
    bool openHelper( const QString& filename, int sqliteFlags );
    bool checkDb();

    sqlite3 *_db;
    QString _error; // last error string
    int _errId;

};

class OWNCLOUDSYNC_EXPORT SqlQuery
{
    Q_DISABLE_COPY(SqlQuery)
public:
    explicit SqlQuery();
    explicit SqlQuery(SqlDatabase& db);
    explicit SqlQuery(const QString& sql, SqlDatabase& db);

    ~SqlQuery();
    QString error() const;

    QString stringValue(int index);
    int intValue(int index);
    quint64 int64Value(int index);
    QByteArray baValue(int index);

    bool isSelect();
    bool isPragma();
    bool exec();
    int  prepare( const QString& sql );
    bool next();
    void bindValue(int pos, const QVariant& value);
    QString lastQuery() const;
    int numRowsAffected();
    void reset();
    void finish();

private:
    sqlite3 *_db;
    sqlite3_stmt *_stmt;
    QString _error;
    int _errId;
    QString _sql;
};

} // namespace OCC

#endif // OWNSQL_H
