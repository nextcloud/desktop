/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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


#include "preparedsqlquerymanager.h"

#include <sqlite3.h>

using namespace OCC;

PreparedSqlQuery::PreparedSqlQuery(SqlQuery *query, bool ok)
    : _query(query)
    , _ok(ok)
{
}

PreparedSqlQuery::~PreparedSqlQuery()
{
    _query->reset_and_clear_bindings();
}

const PreparedSqlQuery PreparedSqlQueryManager::get(PreparedSqlQueryManager::Key key)
{
    auto &query = _queries[key];
    ENFORCE(query._stmt)
    Q_ASSERT(!sqlite3_stmt_busy(query._stmt));
    return { &query };
}

const PreparedSqlQuery PreparedSqlQueryManager::get(PreparedSqlQueryManager::Key key, const QByteArray &sql, SqlDatabase &db)
{
    auto &query = _queries[key];
    Q_ASSERT(!sqlite3_stmt_busy(query._stmt));
    ENFORCE(!query._sqldb || &db == query._sqldb)
    if (!query._stmt) {
        query._sqldb = &db;
        query._db = db.sqliteDb();
        return { &query, query.prepare(sql) == 0 };
    }
    return { &query };
}
