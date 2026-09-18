/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "recipienticonutils.h"

using namespace Qt::StringLiterals;

QString OCC::Gui::Sharing::RecipientIconUtils::svgDataUrl(const QString &svg)
{
    if (svg.isEmpty()) {
        return {};
    }

    return "data:image/svg+xml;base64,%1"_L1.arg(QString::fromLatin1(svg.toUtf8().toBase64()));
}
