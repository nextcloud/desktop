/*
 * Copyright (C) by Kevin Ottens <kevin.ottens@nextcloud.com>
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

#include <QString>

#include "owncloudlib.h"
#include "common/result.h"

namespace OCC {

namespace XAttrWrapper
{

OWNCLOUDSYNC_EXPORT bool hasNextcloudPlaceholderAttributes(const QString &path);
OWNCLOUDSYNC_EXPORT Result<void, QString> addNextcloudPlaceholderAttributes(const QString &path);

}

} // namespace OCC
