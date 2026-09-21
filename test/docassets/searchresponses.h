/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QByteArray>
#include <QUrl>
namespace OCC::DocAssets
{
QByteArray searchResponse(const QUrl &url);
}
