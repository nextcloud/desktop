/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <limits>

#include <QtCore>

namespace OCC {

/**
 * @brief The UnifiedSearchResult class
 * @ingroup gui
 * Simple data structure that represents single Unified Search result
 */

struct UnifiedSearchResult
{
    enum Type : quint8 {
        Default,
        ProviderHeader,
        PartialMatchesHeader,
        FetchMoreTrigger,
        RetryFetchMoreTrigger,
    };

    static QString typeAsString(UnifiedSearchResult::Type type);

    QString _title{};
    QString _subline{};
    QString _providerId{};
    QString _providerName{};
    QString _providerIcon{};
    QString _stableKey{};
    bool _isRounded = false;
    bool _isSelected = false;
    bool _isSelectable = false;
    bool _isPartialMatch = false;
    bool _hasOverflow = false;
    bool _isLoading = false;
    qint32 _order = std::numeric_limits<qint32>::max();
    QUrl _resourceUrl{};
    QString _darkIcons{};
    QString _lightIcons{};
    bool _darkIconsIsThumbnail = false;
    bool _lightIconsIsThumbnail = false;
    Type _type = Type::Default;
};
}
