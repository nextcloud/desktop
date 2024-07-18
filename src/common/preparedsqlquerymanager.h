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

#pragma once

#include "ocsynclib.h"
#include "ownsql.h"
#include "common/asserts.h"

namespace OCC {

class OCSYNC_EXPORT PreparedSqlQuery
{
public:
    ~PreparedSqlQuery();

    operator bool() const { return _ok; }

    SqlQuery *operator->() const
    {
        Q_ASSERT(_ok);
        return _query;
    }

    SqlQuery &operator*() const &
    {
        Q_ASSERT(_ok);
        return *_query;
    }

private:
    PreparedSqlQuery(SqlQuery *query, bool ok = true);

    SqlQuery *_query;
    bool _ok;

    friend class PreparedSqlQueryManager;
};

/**
 * @brief Manage PreparedSqlQuery
 */
class OCSYNC_EXPORT PreparedSqlQueryManager
{
public:
    enum Key {
        GetFileRecordQuery,
        GetFileRecordQueryByInode,
        GetFileRecordQueryByFileId,
        GetFilesBelowPathQuery,
        GetAllFilesQuery,
        ListFilesInPathQuery,
        SetFileRecordQuery,
        SetFileRecordChecksumQuery,
        GetDownloadInfoQuery,
        SetDownloadInfoQuery,
        DeleteDownloadInfoQuery,
        GetUploadInfoQuery,
        GetAllUploadInfoQuery,
        SetUploadInfoQuery,
        DeleteUploadInfoQuery,
        DeleteFileRecordPhash,
        DeleteFileRecordRecursively,
        GetErrorBlacklistQuery,
        SetErrorBlacklistQuery,
        GetSelectiveSyncListQuery,
        GetChecksumTypeIdQuery,
        GetChecksumTypeQuery,
        InsertChecksumTypeQuery,
        GetDataFingerprintQuery,
        SetDataFingerprintQuery1,
        SetDataFingerprintQuery2,
        GetConflictRecordQuery,
        SetConflictRecordQuery,
        DeleteConflictRecordQuery,
        GetRawPinStateQuery,
        GetEffectivePinStateQuery,
        GetSubPinsQuery,
        CountDehydratedFilesQuery,
        SetPinStateQuery,
        WipePinStateQuery,

        GetFileReocrdsWithDirtyPlaceholdersQuery,

        PreparedQueryCount
    };
    PreparedSqlQueryManager() = default;
    /**
     * The queries are reset in the destructor to prevent wal locks
     */
    const PreparedSqlQuery get(Key key);
    /**
     * Prepare the SqlQuery if it was not prepared yet.
     */
    const PreparedSqlQuery get(Key key, const QByteArray &sql, SqlDatabase &db);

private:
    SqlQuery _queries[PreparedQueryCount];
    Q_DISABLE_COPY(PreparedSqlQueryManager)
};

}
