/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2021 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

    explicit operator bool() const { return _ok; }

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
        GetFileRecordQueryByMangledName,
        GetFileRecordQueryByInode,
        GetFileRecordQueryByFileId,
        GetFilesBelowPathQuery,
        GetAllFilesQuery,
        ListFilesInPathQuery,
        SetFileRecordQuery,
        SetFileRecordChecksumQuery,
        SetFileRecordLocalMetadataQuery,
        GetDownloadInfoQuery,
        SetDownloadInfoQuery,
        DeleteDownloadInfoQuery,
        GetUploadInfoQuery,
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
        SetKeyValueStoreQuery,
        GetKeyValueStoreQuery,
        DeleteKeyValueStoreQuery,
        GetConflictRecordQuery,
        SetConflictRecordQuery,
        GetCaseClashConflictRecordQuery,
        GetCaseClashConflictRecordByPathQuery,
        SetCaseClashConflictRecordQuery,
        DeleteCaseClashConflictRecordQuery,
        GetAllCaseClashConflictPathQuery,
        DeleteConflictRecordQuery,
        GetRawPinStateQuery,
        GetEffectivePinStateQuery,
        GetSubPinsQuery,
        CountDehydratedFilesQuery,
        SetPinStateQuery,
        WipePinStateQuery,
        SetE2EeLockedFolderQuery,
        GetE2EeLockedFolderQuery,
        GetE2EeLockedFoldersQuery,
        DeleteE2EeLockedFolderQuery,
        ListAllTopLevelE2eeFoldersStatusLessThanQuery,
        RelocateFolderToNewPathRecursivelyQuery,
        PreparedQueryCount,
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
