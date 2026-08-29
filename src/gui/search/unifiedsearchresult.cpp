/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "unifiedsearchresult.h"

#include <QString>

namespace OCC {

QString UnifiedSearchResult::typeAsString(UnifiedSearchResult::Type type)
{
    QString result;

    switch (type) {
    case Default:
        result = QStringLiteral("Default");
        break;

    case ProviderHeader:
        result = QStringLiteral("ProviderHeader");
        break;

    case PartialMatchesHeader:
        result = QStringLiteral("PartialMatchesHeader");
        break;

    case FetchMoreTrigger:
        result = QStringLiteral("FetchMoreTrigger");
        break;

    case RetryFetchMoreTrigger:
        result = QStringLiteral("RetryFetchMoreTrigger");
        break;
    }
    return result;
}
}
