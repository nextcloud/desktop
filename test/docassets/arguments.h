/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QStringList>

namespace OCC::DocAssets
{
QStringList normalizedCaptureArguments(const QStringList &arguments, QString *error);
}
