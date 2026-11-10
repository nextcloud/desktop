/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QString>

namespace OCC::DocAssets
{
bool prepareEnvironment(const QString &configurationDirectory);
void registerWizardTypes();
}
