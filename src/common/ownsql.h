/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef OWNSQL_H
#define OWNSQL_H

#include <QLoggingCategory>
#include <QObject>
#include <QVariant>

#include "ocsynclib.h"

struct sqlite3;
struct sqlite3_stmt;

namespace OCC {
OCSYNC_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcSql)

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
    [[nodiscard]] QString error() const;
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

    sqlite3 *_db = nullptr;
    QString _error; // last error string
    int _errId = 0;

    friend class SqlQuery;
    QSet<SqlQuery *> _queries;
};

/**
 * @brief The SqlQuery class
 * @ingroup libsync
 *
 * There is basically 3 ways to initialize and use a query:
 *
    SqlQuery q1;
    [...]
    q1.initOrReset(...);
    q1.bindValue(...);
    q1.exec(...)
 *
    SqlQuery q2(db);
    q2.prepare(...);
    [...]
    q2.reset_and_clear_bindings();
    q2.bindValue(...);
    q2.exec(...)
 *
    SqlQuery q3("...", db);
    q3.bindValue(...);
    q3.exec(...)
 *
 */
class OCSYNC_EXPORT SqlQuery
{
    Q_DISABLE_COPY(SqlQuery)
public:
    explicit SqlQuery() = default;
    explicit SqlQuery(SqlDatabase &db);
    explicit SqlQuery(const QByteArray &sql, SqlDatabase &db);
    /**
     * Prepare the SqlQuery.
     * If the query was already prepared, this will first call finish(), and re-prepare it.
     * This function must only be used if the constructor was setting a SqlDatabase
     */
    int prepare(const QByteArray &sql, bool allow_failure = false);

    ~SqlQuery();
    [[nodiscard]] QString error() const;
    [[nodiscard]] int errorId() const;

    /// Checks whether the value at the given column index is NULL
    bool nullValue(int index);

    QString stringValue(int index);
    int intValue(int index);
    quint64 int64Value(int index);
    QByteArray baValue(int index);
    bool isSelect();
    bool isPragma();
    bool exec();

    struct NextResult
    {
        bool ok = false;
        bool hasData = false;
    };
    NextResult next();

    template<class T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
    void bindValue(int pos, const T &value)
    {
        bindValueInternal(pos, static_cast<int>(value));
    }

    template<class T, typename std::enable_if<!std::is_enum<T>::value, int>::type = 0>
    void bindValue(int pos, const T &value)
    {
        bindValueInternal(pos, value);
    }

    void bindValue(int pos, const QByteArray &value)
    {
        bindValueInternal(pos, value);
    }

    [[nodiscard]] const QByteArray &lastQuery() const;
    int numRowsAffected();
    void reset_and_clear_bindings();

private:
    void bindValueInternal(int pos, const QVariant &value);
    void finish();

    SqlDatabase *_sqldb = nullptr;
    sqlite3 *_db = nullptr;
    sqlite3_stmt *_stmt = nullptr;
    QString _error;
    int _errId = 0;
    QByteArray _sql;

    friend class SqlDatabase;
    friend class PreparedSqlQueryManager;
};

} // namespace OCC

#endif // OWNSQL_H
