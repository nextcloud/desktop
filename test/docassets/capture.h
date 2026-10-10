/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "catalogue.h"
#include <QQmlError>

namespace OCC::DocAssets
{
bool captureScenario(const QString &scenario, const QString &stagingDirectory, int timeoutMs, QString *error, const QUrl &sourceOverride = {});
void recordQmlWarnings(const QList<QQmlError> &messages, QString *error);
}
