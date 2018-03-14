/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef OWNSQL_H
#define OWNSQL_H

#include <QObject>
#include <QVariant>

#include "ocsynclib.h"

struct sqlite3;
struct sqlite3_stmt;

namespace OCC {

class SqlQuery;

/**
 * @brief The SqlDatabase class
 * @ingroup libsync
 */
class OCSYNC_EXPORT SqlDatabase
{
    Q_DISABLE_COPY(SqlDatabase)
public:
    explicit SqlDatabase();
    ~SqlDatabase();

    bool isOpen();
    bool openOrCreateReadWrite(const QString &filename);
    bool openReadOnly(const QString &filename);
    bool transaction();
    bool commit();
    void close();
    QString error() const;
    sqlite3 *sqliteDb();

private:
    enum class CheckDbResult {
        Ok,
        CantPrepare,
        CantExec,
        NotOk,
    };

    bool openHelper(const QString &filename, int sqliteFlags);
    CheckDbResult checkDb();

    sqlite3 *_db;
    QString _error; // last error string
    int _errId;

    friend class SqlQuery;
    QVector<SqlQuery *> _queries;
};

/**
 * @brief The SqlQuery class
 * @ingroup libsync
 */
class OCSYNC_EXPORT SqlQuery
{
    Q_DISABLE_COPY(SqlQuery)
public:
    explicit SqlQuery(SqlDatabase &db);
    explicit SqlQuery(const QByteArray &sql, SqlDatabase &db);

    ~SqlQuery();
    QString error() const;
    int errorId() const;

    /// Checks whether the value at the given column index is NULL
    bool nullValue(int index);

    QString stringValue(int index);
    int intValue(int index);
    quint64 int64Value(int index);
    QByteArray baValue(int index);

    bool isSelect();
    bool isPragma();
    bool exec();
    int prepare(const QByteArray &sql, bool allow_failure = false);
    bool next();
    void bindValue(int pos, const QVariant &value);
    QString lastQuery() const;
    int numRowsAffected();
    void reset_and_clear_bindings();
    void finish();

private:
    SqlDatabase *_sqldb = nullptr;
    sqlite3 *_db = nullptr;
    sqlite3_stmt *_stmt = nullptr;
    QString _error;
    int _errId;
    QByteArray _sql;
};

} // namespace OCC

#endif // OWNSQL_H
