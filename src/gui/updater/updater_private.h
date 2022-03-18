/*
 * Copyright (C) 2022 by Fabian Müller <fmueller@owncloud.com>
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

namespace OCC {
static const QString updateAvailableC = "Updater/updateAvailable";
static const QString updateTargetVersionC = "Updater/updateTargetVersion";
static const QString updateTargetVersionStringC = "Updater/updateTargetVersionString";
// the config file key's name is preserved for legacy reasons
static const QString previouslySkippedVersionC = "Updater/seenVersion";
static const QString autoUpdateAttemptedC = "Updater/autoUpdateAttempted";
}
