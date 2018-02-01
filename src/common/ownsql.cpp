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

#include <QDateTime>
#include <QLoggingCategory>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QDir>

#include "ownsql.h"
#include "common/utility.h"
#include "common/asserts.h"
#include <sqlite3.h>

#define SQLITE_SLEEP_TIME_USEC 100000
#define SQLITE_REPEAT_COUNT 20

#define SQLITE_DO(A)                                         \
    if (1) {                                                 \
        _errId = (A);                                        \
        if (_errId != SQLITE_OK && _errId != SQLITE_DONE) {  \
            _error = QString::fromUtf8(sqlite3_errmsg(_db)); \
        }                                                    \
    }

namespace OCC {

Q_LOGGING_CATEGORY(lcSql, "sync.database.sql", QtInfoMsg)

SqlDatabase::SqlDatabase()
    : _db(0)
    , _errId(0)
{
}

bool SqlDatabase::isOpen()
{
    return _db != 0;
}

bool SqlDatabase::openHelper(const QString &filename, int sqliteFlags)
{
    if (isOpen()) {
        return true;
    }

    sqliteFlags |= SQLITE_OPEN_NOMUTEX;

    SQLITE_DO(sqlite3_open_v2(filename.toUtf8().constData(), &_db, sqliteFlags, 0));

    if (_errId != SQLITE_OK) {
        qCWarning(lcSql) << "Error:" << _error << "for" << filename;
        if (_errId == SQLITE_CANTOPEN) {
            qCWarning(lcSql) << "CANTOPEN extended errcode: " << sqlite3_extended_errcode(_db);
#if SQLITE_VERSION_NUMBER >= 3012000
            qCWarning(lcSql) << "CANTOPEN system errno: " << sqlite3_system_errno(_db);
#endif
        }
        close();
        return false;
    }

    if (!_db) {
        qCWarning(lcSql) << "Error: no database for" << filename;
        return false;
    }

    sqlite3_busy_timeout(_db, 5000);

    return true;
}

SqlDatabase::CheckDbResult SqlDatabase::checkDb()
{
    // quick_check can fail with a disk IO error when diskspace is low
    SqlQuery quick_check(*this);

    if (quick_check.prepare("PRAGMA quick_check;", /*allow_failure=*/true) != SQLITE_OK) {
        qCWarning(lcSql) << "Error preparing quick_check on database";
        _errId = quick_check.errorId();
        _error = quick_check.error();
        return CheckDbResult::CantPrepare;
    }
    if (!quick_check.exec()) {
        qCWarning(lcSql) << "Error running quick_check on database";
        _errId = quick_check.errorId();
        _error = quick_check.error();
        return CheckDbResult::CantExec;
    }

    quick_check.next();
    QString result = quick_check.stringValue(0);
    if (result != "ok") {
        qCWarning(lcSql) << "quick_check returned failure:" << result;
        return CheckDbResult::NotOk;
    }

    return CheckDbResult::Ok;
}

bool SqlDatabase::openOrCreateReadWrite(const QString &filename)
{
    if (isOpen()) {
        return true;
    }

    if (!openHelper(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)) {
        return false;
    }

    auto checkResult = checkDb();
    if (checkResult != CheckDbResult::Ok) {
        if (checkResult == CheckDbResult::CantPrepare) {
            // When disk space is low, preparing may fail even though the db is fine.
            // Typically CANTOPEN or IOERR.
            qint64 freeSpace = Utility::freeDiskSpace(QFileInfo(filename).dir().absolutePath());
            if (freeSpace != -1 && freeSpace < 1000000) {
                qCWarning(lcSql) << "Can't prepare consistency check and disk space is low:" << freeSpace;
                close();
                return false;
            }

            // Even when there's enough disk space, it might very well be that the
            // file is on a read-only filesystem and can't be opened because of that.
            if (_errId == SQLITE_CANTOPEN) {
                qCWarning(lcSql) << "Can't open db to prepare consistency check, aborting";
                close();
                return false;
            }
        }

        qCCritical(lcSql) << "Consistency check failed, removing broken db" << filename;
        close();
        QFile::remove(filename);

        return openHelper(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    }

    return true;
}

bool SqlDatabase::openReadOnly(const QString &filename)
{
    if (isOpen()) {
        return true;
    }

    if (!openHelper(filename, SQLITE_OPEN_READONLY)) {
        return false;
    }

    if (checkDb() != CheckDbResult::Ok) {
        qCWarning(lcSql) << "Consistency check failed in readonly mode, giving up" << filename;
        close();
        return false;
    }

    return true;
}

QString SqlDatabase::error() const
{
    const QString err(_error);
    // _error.clear();
    return err;
}

void SqlDatabase::close()
{
    if (_db) {
        SQLITE_DO(sqlite3_close(_db));
        if (_errId != SQLITE_OK)
            qCWarning(lcSql) << "Closing database failed" << _error;
        _db = 0;
    }
}

bool SqlDatabase::transaction()
{
    if (!_db) {
        return false;
    }
    SQLITE_DO(sqlite3_exec(_db, "BEGIN", 0, 0, 0));
    return _errId == SQLITE_OK;
}

bool SqlDatabase::commit()
{
    if (!_db) {
        return false;
    }
    SQLITE_DO(sqlite3_exec(_db, "COMMIT", 0, 0, 0));
    return _errId == SQLITE_OK;
}

sqlite3 *SqlDatabase::sqliteDb()
{
    return _db;
}

/* =========================================================================================== */

SqlQuery::SqlQuery(SqlDatabase &db)
    : _db(db.sqliteDb())
    , _stmt(0)
    , _errId(0)
{
}

SqlQuery::~SqlQuery()
{
    if (_stmt) {
        finish();
    }
}

SqlQuery::SqlQuery(const QString &sql, SqlDatabase &db)
    : _db(db.sqliteDb())
    , _stmt(0)
    , _errId(0)
{
    prepare(sql);
}

int SqlQuery::prepare(const QString &sql, bool allow_failure)
{
    QString s(sql);
    _sql = s.trimmed();
    if (_stmt) {
        finish();
    }
    if (!_sql.isEmpty()) {
        int n = 0;
        int rc;
        do {
            rc = sqlite3_prepare_v2(_db, _sql.toUtf8().constData(), -1, &_stmt, 0);
            if ((rc == SQLITE_BUSY) || (rc == SQLITE_LOCKED)) {
                n++;
                OCC::Utility::usleep(SQLITE_SLEEP_TIME_USEC);
            }
        } while ((n < SQLITE_REPEAT_COUNT) && ((rc == SQLITE_BUSY) || (rc == SQLITE_LOCKED)));
        _errId = rc;

        if (_errId != SQLITE_OK) {
            _error = QString::fromUtf8(sqlite3_errmsg(_db));
            qCWarning(lcSql) << "Sqlite prepare statement error:" << _error << "in" << _sql;
            ENFORCE(allow_failure, "SQLITE Prepare error");
        }
    }
    return _errId;
}

bool SqlQuery::isSelect()
{
    return (!_sql.isEmpty() && _sql.startsWith("SELECT", Qt::CaseInsensitive));
}

bool SqlQuery::isPragma()
{
    return (!_sql.isEmpty() && _sql.startsWith("PRAGMA", Qt::CaseInsensitive));
}

bool SqlQuery::exec()
{
    qCDebug(lcSql) << "SQL exec" << _sql;

    if (!_stmt) {
        qCWarning(lcSql) << "Can't exec query, statement unprepared.";
        return false;
    }

    // Don't do anything for selects, that is how we use the lib :-|
    if (!isSelect() && !isPragma()) {
        int rc, n = 0;
        do {
            rc = sqlite3_step(_stmt);
            if (rc == SQLITE_LOCKED) {
                rc = sqlite3_reset(_stmt); /* This will also return SQLITE_LOCKED */
                n++;
                OCC::Utility::usleep(SQLITE_SLEEP_TIME_USEC);
            } else if (rc == SQLITE_BUSY) {
                OCC::Utility::usleep(SQLITE_SLEEP_TIME_USEC);
                n++;
            }
        } while ((n < SQLITE_REPEAT_COUNT) && ((rc == SQLITE_BUSY) || (rc == SQLITE_LOCKED)));
        _errId = rc;

        if (_errId != SQLITE_DONE && _errId != SQLITE_ROW) {
            _error = QString::fromUtf8(sqlite3_errmsg(_db));
            qCWarning(lcSql) << "Sqlite exec statement error:" << _errId << _error << "in" << _sql;
            if (_errId == SQLITE_IOERR) {
                qCWarning(lcSql) << "IOERR extended errcode: " << sqlite3_extended_errcode(_db);
#if SQLITE_VERSION_NUMBER >= 3012000
                qCWarning(lcSql) << "IOERR system errno: " << sqlite3_system_errno(_db);
#endif
            }
        } else {
            qCDebug(lcSql) << "Last exec affected" << numRowsAffected() << "rows.";
        }
        return (_errId == SQLITE_DONE); // either SQLITE_ROW or SQLITE_DONE
    }

    return true;
}

bool SqlQuery::next()
{
    SQLITE_DO(sqlite3_step(_stmt));
    return _errId == SQLITE_ROW;
}

void SqlQuery::bindValue(int pos, const QVariant &value)
{
    qCDebug(lcSql) << "SQL bind" << pos << value;

    int res = -1;
    if (!_stmt) {
        ASSERT(false);
        return;
    }

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
    case QVariant::ULongLong:
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
        if (!value.toString().isNull()) {
            // lifetime of string == lifetime of its qvariant
            const QString *str = static_cast<const QString *>(value.constData());
            res = sqlite3_bind_text16(_stmt, pos, str->utf16(),
                (str->size()) * sizeof(QChar), SQLITE_TRANSIENT);
        } else {
            res = sqlite3_bind_null(_stmt, pos);
        }
        break;
    }
    case QVariant::ByteArray: {
        auto ba = value.toByteArray();
        res = sqlite3_bind_text(_stmt, pos, ba.constData(), ba.size(), SQLITE_TRANSIENT);
        break;
    }
    default: {
        QString str = value.toString();
        // SQLITE_TRANSIENT makes sure that sqlite buffers the data
        res = sqlite3_bind_text16(_stmt, pos, str.utf16(),
            (str.size()) * sizeof(QChar), SQLITE_TRANSIENT);
        break;
    }
    }
    if (res != SQLITE_OK) {
        qCWarning(lcSql) << "ERROR binding SQL value:" << value << "error:" << res;
    }
    ASSERT(res == SQLITE_OK);
}

bool SqlQuery::nullValue(int index)
{
    return sqlite3_column_type(_stmt, index) == SQLITE_NULL;
}

QString SqlQuery::stringValue(int index)
{
    return QString::fromUtf16(static_cast<const ushort *>(sqlite3_column_text16(_stmt, index)));
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
    return QByteArray(static_cast<const char *>(sqlite3_column_blob(_stmt, index)),
        sqlite3_column_bytes(_stmt, index));
}

QString SqlQuery::error() const
{
    return _error;
}

int SqlQuery::errorId() const
{
    return _errId;
}

QString SqlQuery::lastQuery() const
{
    return _sql;
}

int SqlQuery::numRowsAffected()
{
    return sqlite3_changes(_db);
}

void SqlQuery::finish()
{
    SQLITE_DO(sqlite3_finalize(_stmt));
    _stmt = 0;
}

void SqlQuery::reset_and_clear_bindings()
{
    if (_stmt) {
        SQLITE_DO(sqlite3_reset(_stmt));
        SQLITE_DO(sqlite3_clear_bindings(_stmt));
    }
}

} // namespace OCC
