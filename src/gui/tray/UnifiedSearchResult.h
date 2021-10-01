/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef UNIFIEDSEARHRESULT_H
#define UNIFIEDSEARHRESULT_H

#include <limits>

#include <QtCore>

namespace OCC {

/**
 * @brief The UnifiedSearchResult class
 * @ingroup gui
 * Simple data structure that represents single Unified Search result
 */

class UnifiedSearchResult
{
public:
    enum Type : quint8 { Default = 0, CategorySeparator, FetchMoreTrigger };

    static QString typeAsString(UnifiedSearchResult::Type type);

    QString _title;
    QString _subline;
    QString _providerId;
    QString _providerName;
    bool _isRounded = false;
    qint32 _order = std::numeric_limits<quint32>::max();
    QString _resourceUrl;
    QString _icons;
    Type _type = Type::Default;
};
}

#endif // UNIFIEDSEARHRESULT_H
