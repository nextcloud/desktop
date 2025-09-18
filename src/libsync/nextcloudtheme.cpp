/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2012 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nextcloudtheme.h"

#include <QString>
#include <QVariant>
#ifndef TOKEN_AUTH_ONLY
#include <QPixmap>
#include <QIcon>
#endif
#include <QCoreApplication>

#include "config.h"
#include "common/utility.h"
#include "version.h"

namespace OCC {

NextcloudTheme::NextcloudTheme()
    : Theme()
{
}

QString NextcloudTheme::wizardUrlHint() const
{
    return QStringLiteral("https://try.nextcloud.com");
}

}
