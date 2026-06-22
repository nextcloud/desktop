/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "plugin.h"

#include "config.h"

namespace OCC {

PluginFactory::~PluginFactory() = default;

QString pluginFileName(const QString &type, const QString &name)
{
    return QStringLiteral("%1sync_%2_%3")
        .arg(QStringLiteral(APPLICATION_EXECUTABLE), type, name);
}

}
