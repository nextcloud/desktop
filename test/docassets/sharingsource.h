/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QJsonDocument>
#include <QVariantMap>

namespace OCC::DocAssets
{
// Initial properties for the production sharing window, without a local file dependency.
QVariantMap sharingSourceProperties(const QJsonDocument &response, QString *error);
}
