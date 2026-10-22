/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "catalogue.h"
namespace OCC::DocAssets
{
bool exportCatalogue(const QString &executable,
                     const QString &output,
                     const QStringList &scenarios,
                     QString *error,
                     int timeoutMs = captureTimeoutMs + workerStartupTimeoutMs,
                     bool includeIcons = false);
}
