/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "fileprovidereditlocallyjob.h"

#include <QLoggingCategory>

namespace OCC::Mac {

Q_LOGGING_CATEGORY(lcFileProviderEditLocallyJob, "nextcloud.gui.fileprovidereditlocally", QtInfoMsg)

FileProviderEditLocallyJob::FileProviderEditLocallyJob(const AccountStatePtr &accountState,
                                                       const QString &relPath,
                                                       QObject *const parent)
    : QObject(parent)
    , _accountState(accountState)
    , _relPath(relPath)
{
}

} // namespace OCC::Mac
