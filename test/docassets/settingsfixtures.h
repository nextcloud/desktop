/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "generalsettingsservices.h"
namespace OCC::DocAssets
{
class CaptureScene;
GeneralSettingsServices settingsServices();
bool prepareSettings(CaptureScene &scene, const QString &scenario, QString *error);
}
