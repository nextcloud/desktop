/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QString>

#include "owncloudlib.h"
#include "common/result.h"

#include "xattrexport.h"

namespace OCC {

namespace XAttrWrapper
{

OWNCLOUDSYNC_EXPORT bool hasNextcloudPlaceholderAttributes(const QString &path);
OWNCLOUDSYNC_EXPORT Result<void, QString> addNextcloudPlaceholderAttributes(const QString &path);

}

} // namespace OCC
