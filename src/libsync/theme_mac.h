/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QIcon>

namespace OCC {

/**
 * Build a QIcon from the AppIcon asset compiled into the app bundle's
 * Assets.car by `actool`. Returns an empty QIcon when run outside of an
 * assembled bundle (e.g. development binaries).
 */
QIcon loadAppIconFromBundle();

}
