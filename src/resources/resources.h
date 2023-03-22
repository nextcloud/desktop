/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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
#include "resources/owncloudresources.h"

#include <QIcon>

namespace OCC::Resources {
/**
 * Whether use the dark icon theme
 * The function also ensures the theme supports the dark theme
 */
bool OWNCLOUDRESOURCES_EXPORT isUsingDarkTheme();

QIcon OWNCLOUDRESOURCES_EXPORT getCoreIcon(const QString &icon_name);
}
