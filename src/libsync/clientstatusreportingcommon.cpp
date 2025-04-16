/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "clientstatusreportingcommon.h"
#include <QLoggingCategory>

namespace OCC {
Q_LOGGING_CATEGORY(lcClientStatusReportingCommon, "nextcloud.sync.clientstatusreportingcommon", QtInfoMsg)

QByteArray clientStatusstatusStringFromNumber(const ClientStatusReportingStatus status)
{
    Q_ASSERT(static_cast<int>(status) >= 0 && static_cast<int>(status) < static_cast<int>(ClientStatusReportingStatus::Count));
    if (static_cast<int>(status) < 0 || static_cast<int>(status) >= static_cast<int>(ClientStatusReportingStatus::Count)) {
        qCDebug(lcClientStatusReportingCommon) << "Invalid status:" << static_cast<int>(status);
        return {};
    }

    switch (status) {
    case ClientStatusReportingStatus::DownloadError_ConflictCaseClash:
        return QByteArrayLiteral("DownloadError.CONFLICT_CASECLASH");
    case ClientStatusReportingStatus::DownloadError_ConflictInvalidCharacters:
        return QByteArrayLiteral("DownloadError.CONFLICT_INVALID_CHARACTERS");
    case ClientStatusReportingStatus::DownloadError_ServerError:
        return QByteArrayLiteral("DownloadError.SERVER_ERROR");
    case ClientStatusReportingStatus::DownloadError_Virtual_File_Hydration_Failure:
        return QByteArrayLiteral("DownloadError.VIRTUAL_FILE_HYDRATION_FAILURE");
    case ClientStatusReportingStatus::E2EeError_GeneralError:
        return QByteArrayLiteral("E2EeError.General");
    case ClientStatusReportingStatus::UploadError_ServerError:
        return QByteArrayLiteral("UploadError.SERVER_ERROR");
    case ClientStatusReportingStatus::UploadError_Virus_Detected:
        return QByteArrayLiteral("UploadResult.VIRUS_DETECTED");
    case ClientStatusReportingStatus::Count:
        return {};
    };
    return {};
}
}
