/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
#pragma once

#include "owncloudlib.h"
#include <QtCore/qbytearray.h>

namespace OCC {
enum class ClientStatusReportingStatus {
    DownloadError_Cannot_Create_File = 0,
    DownloadError_Conflict,
    DownloadError_ConflictCaseClash,
    DownloadError_ConflictInvalidCharacters,
    DownloadError_No_Free_Space,
    DownloadError_ServerError,
    DownloadError_Virtual_File_Hydration_Failure,
    E2EeError_GeneralError,
    UploadError_Conflict,
    UploadError_ConflictInvalidCharacters,
    UploadError_No_Free_Space,
    UploadError_No_Write_Permissions,
    UploadError_ServerError,
    UploadError_Virus_Detected,
    Count,
};
QByteArray OWNCLOUDSYNC_EXPORT clientStatusstatusStringFromNumber(const ClientStatusReportingStatus status);
}
