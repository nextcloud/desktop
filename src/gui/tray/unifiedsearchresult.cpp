/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore>

#include "unifiedsearchresult.h"

namespace OCC {

QString UnifiedSearchResult::typeAsString(UnifiedSearchResult::Type type)
{
    QString result;

    switch (type) {
    case Default:
        result = QStringLiteral("Default");
        break;

    case FetchMoreTrigger:
        result = QStringLiteral("FetchMoreTrigger");
        break;
    }
    return result;
}
}
