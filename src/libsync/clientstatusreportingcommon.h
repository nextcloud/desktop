/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "owncloudlib.h"
#include <QtCore/qbytearray.h>

namespace OCC {
enum class ClientStatusReportingStatus {
    DownloadError_ConflictCaseClash = 0,
    DownloadError_ConflictInvalidCharacters,
    DownloadError_ServerError,
    DownloadError_Virtual_File_Hydration_Failure,
    E2EeError_GeneralError,
    UploadError_ServerError,
    UploadError_Virus_Detected,
    Count,
};
QByteArray OWNCLOUDSYNC_EXPORT clientStatusstatusStringFromNumber(const ClientStatusReportingStatus status);
}
